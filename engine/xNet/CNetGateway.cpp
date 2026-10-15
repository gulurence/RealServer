#include "CNetGateway.h"
#include "xNetAgent.h"
#include "xActor/ActorMgr.h"
#include "xActor/Actor.h"
#include "xActor/ActorMsgId.h"
#include "xCluster/xProcessDefine.h"
#include "xRedis/RedisMgr.h"
#include "xRedis/RedisCli.h"
#include "xLog/xLog.h"

#include <cstring>
#include <cstdlib>
#include <functional>

// =============================================================================
// 客户端帧 (Phase 5): [uint32 u32Len][protobuf body]
// body 前两个字段为 BigType(一级 Actor 类型) / SmallType(二级子模块) 用于路由。
// 路由规则: msgId = (BigType << 16) | SmallType, 投递给 Gate Actor 的 on_msg。
// on_msg 收到的 body = [8 字节 CID][protobuf body] (小端 CID), 供回包 SendToCID。
// =============================================================================

namespace
{

// 读取一个 protobuf varint (返回 false 表示数据不足/截断)
bool ReadVarint(const uint8 *p, size_t len, size_t &pos, uint64 &value)
{
    value = 0;
    int32 shift = 0;
    while (pos < len && shift < 64)
    {
        uint8 b = p[pos++];
        value |= (uint64)(b & 0x7F) << shift;
        if (!(b & 0x80))
        {
            return true;
        }
        shift += 7;
    }
    return false;
}

// 从 protobuf body 解析前两个 varint 字段: big_type(field 1) / small_type(field 2)。
// 只读前导字段, 跳过长度定界字段 (string/token), 不解析完整消息。
void ParseRouteFields(const uint8 *body, size_t len, uint32 &bigType, uint32 &smallType)
{
    bigType = 0;
    smallType = 0;
    size_t pos = 0;
    uint64 key = 0;
    uint64 val = 0;
    while (pos < len && ReadVarint(body, len, pos, key))
    {
        uint32 fieldNum = (uint32)(key >> 3);
        uint32 wireType = (uint32)(key & 0x7);
        if (wireType == 0)
        { // varint
            if (!ReadVarint(body, len, pos, val))
            {
                break;
            }
            if (fieldNum == 1)
            {
                bigType = (uint32)val;
            }
            if (fieldNum == 2)
            {
                smallType = (uint32)val;
            }
        }
        else if (wireType == 2)
        { // length-delimited
            if (!ReadVarint(body, len, pos, val))
            {
                break;
            }
            pos += (size_t)val;
            if (pos > len)
            {
                break;
            }
        }
        else if (wireType == 1)
        { // fixed64
            pos += 8;
            if (pos > len)
            {
                break;
            }
        }
        else if (wireType == 5)
        { // fixed32
            pos += 4;
            if (pos > len)
            {
                break;
            }
        }
        else
        {
            break; // 未知 wire type, 停止
        }
    }
}

} // namespace

CNetGateway::CNetGateway()
{
}

CNetGateway::~CNetGateway()
{
}

bool CNetGateway::Start(uint16 u16Port)
{
    // 取得本进程 Redis 连接 (actor_cache 池): 读限流阈值
    m_pRedis = CRedisPoolMgr::getMe().GetRedisCli("actor_cache");

    // 配置网络回调：包到达后回调 OnPackage 完成路由
    m_stConfig.strServiceName = "ActorNet";
    m_stConfig.strHost = "0.0.0.0";
    m_stConfig.u16Port = u16Port;
    m_stConfig.u16CacheBuffSize = 32 * 1024; // 32KB 接收缓冲 (uint16 上限 65535)
    m_stConfig.pParam = this;

    m_stConfig.pAcceptCall
        = std::bind(&CNetGateway::OnAccept, this, std::placeholders::_1, std::placeholders::_2);
    m_stConfig.pPackageCall
        = std::bind(&CNetGateway::OnPackage, this, std::placeholders::_1, std::placeholders::_2,
                    std::placeholders::_3, std::placeholders::_4);
    m_stConfig.pDisconnect
        = std::bind(&CNetGateway::OnDisconnect, this, std::placeholders::_1, std::placeholders::_2);
    m_stConfig.pSocketReady = std::bind(&CNetGateway::OnSocketReady, this, std::placeholders::_1,
                                        std::placeholders::_2, std::placeholders::_3);

    m_pAgent = std::make_unique<xNetAgent>(m_stConfig);
    if (!m_pAgent->thread_start())
    {
        XERR("[NetGateway] failed to start network thread");
        m_pAgent.reset();
        return false;
    }

    XLOG("[NetGateway] listening on 0.0.0.0:%u", u16Port);
    return true;
}

void CNetGateway::Stop()
{
    // xThread 以 detach 方式启动，xNetAgent::thread_proc 内 io_context 无外部
    // stop 接口；此处仅占位 (进程主循环为死循环，正常不走到 Stop)。
    XLOG("[NetGateway] stop requested (no-op — detached thread)");
}

NetCID CNetGateway::OnAccept(void *, tcp_socket &)
{
    return m_nextCID.fetch_add(1);
}

void CNetGateway::OnSocketReady(void *, NetCID u64CID, std::shared_ptr<tcp_socket> pSocket)
{
    std::lock_guard<std::mutex> lock(m_sockMutex);
    if (pSocket)
    {
        m_mapSockets[u64CID] = pSocket;
        XDBG("[NetGateway] socket registered cid=%lld", u64CID);
    }
    else
    {
        m_mapSockets.erase(u64CID);
        XDBG("[NetGateway] socket unregistered cid=%lld", u64CID);
    }
}

void CNetGateway::OnPackage(void *, tcp_socket &, uint64 u64CID, NetPackageSharedPtr pPackage)
{
    if (!pPackage || !pPackage->pPackageData || pPackage->u32Size < ClientFrameHeadSize)
    {
        return;
    }

    // 单连接消息接收限流: 先刷新阈值 (5s TTL 缓存读 Redis), 超限直接断开连接
    GetRateLimitPerSec();
    if (!m_limiter.Consume(u64CID))
    {
        XWRN("[NetGateway] rate limit exceeded cid=%lld — disconnect", u64CID);
        Kick(u64CID);
        return;
    }

    // 帧: [uint32 u32Len][protobuf body]
    const uint8 *pData = pPackage->pPackageData;
    uint32 u32BodyLen = 0;
    std::memcpy(&u32BodyLen, pData, ClientFrameHeadSize);
    if (u32BodyLen == 0 || u32BodyLen > pPackage->u32Size - ClientFrameHeadSize)
    {
        XWRN("[NetGateway] cid=%lld bad frame len=%u", u64CID, u32BodyLen);
        return;
    }
    const uint8 *pBody = pData + ClientFrameHeadSize;

    // 读 BigType / SmallType (body 前两个 varint 字段)
    uint32 bigType = 0;
    uint32 smallType = 0;
    ParseRouteFields(pBody, u32BodyLen, bigType, smallType);

    // 组装 msgId = (BigType << 16) | SmallType
    uint32 u32MsgId = (bigType << 16) | (smallType & 0xFFFF);

    // 路由到 Gate Actor (客户端唯一入口)
    CActor *pActor = CActorMgr::instance()->Find(EActorType_AGate, 0);
    if (!pActor)
    {
        XWRN("[NetGateway] cid=%lld drop big=%u small=%u — Gate actor not installed", u64CID,
             bigType, smallType);
        return;
    }

    // 回包需要 CID, 将其以小端 8 字节前置到 protobuf body
    std::string routedBody;
    routedBody.reserve(sizeof(uint64) + u32BodyLen);
    uint8 cidBytes[8];
    uint64 c = (uint64)u64CID;
    for (int32 i = 0; i < 8; ++i)
    {
        cidBytes[i] = (uint8)(c >> (8 * i));
    }
    routedBody.append(reinterpret_cast<const char *>(cidBytes), 8);
    routedBody.append(reinterpret_cast<const char *>(pBody), u32BodyLen);

    pActor->PostMessage(u32MsgId, routedBody);
    XDBG("[NetGateway] cid=%lld msg=%u (big=%u small=%u) -> Gate body=%u bytes", u64CID, u32MsgId,
         bigType, smallType, u32BodyLen);
}

void CNetGateway::OnDisconnect(void *, NetCID u64CID)
{
    m_limiter.Reset(u64CID);
    XLOG("[NetGateway] client disconnect cid=%lld", u64CID);

    // 断线是重要事件: 投递到 Gate Actor 统一收尾 (连接映射清理 / 房间按离开处理 /
    // Player Actor 状态置离线)。沿用 OnPackage 的 8 字节小端 CID 前置约定, Lua 侧
    // 用 si_net_body_cid 还原。socket 注销由 xNetAgent 断开分支调 pSocketReady(nullptr) 完成。
    CActor *pActor = CActorMgr::instance()->Find(EActorType_AGate, 0);
    if (!pActor)
    {
        XWRN("[NetGateway] disconnect cid=%lld — Gate actor not installed, skip cleanup", u64CID);
        return;
    }

    uint8 cidBytes[8] = {0};
    uint64 c = (uint64)u64CID;
    for (int32 i = 0; i < 8; ++i)
    {
        cidBytes[i] = (uint8)(c >> (8 * i));
    }
    std::string body(reinterpret_cast<const char *>(cidBytes), 8);
    pActor->PostMessage(xMsgId::SYS_DISCONNECT, body);
}

bool CNetGateway::SendToCID(NetCID u64CID, const std::string &strBody)
{
    std::shared_ptr<tcp_socket> sock;
    {
        std::lock_guard<std::mutex> lock(m_sockMutex);
        auto it = m_mapSockets.find(u64CID);
        if (it == m_mapSockets.end() || !it->second)
        {
            XWRN("[NetGateway] SendToCID cid=%lld — no socket", u64CID);
            return false;
        }
        sock = it->second;
    }

    // 帧: [uint32 len][body]
    uint32 u32Len = (uint32)strBody.size();
    std::string frame;
    frame.reserve(ClientFrameHeadSize + u32Len);
    uint8 lenBytes[4];
    for (int32 i = 0; i < 4; ++i)
    {
        lenBytes[i] = (uint8)(u32Len >> (8 * i));
    }
    frame.append(reinterpret_cast<const char *>(lenBytes), 4);
    frame.append(strBody);

    // 投递到 socket 所属 executor 同步写 (线程安全, 不阻塞调用线程)
    auto data = std::make_shared<std::string>(std::move(frame));
    auto executor = sock->get_executor();
    asio::post(executor,
               [sock, data]()
               {
                   asio::error_code ec;
                   asio::write(*sock, asio::buffer(*data), ec);
                   if (ec)
                   {
                       XERR("[NetGateway] SendToCID write failed: %s", ec.message().c_str());
                   }
               });
    return true;
}

int32 CNetGateway::GetRateLimitPerSec()
{
    std::lock_guard<std::mutex> lock(m_rateMutex);
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<double>(now - m_lastRateRead).count() < 5.0)
    {
        return m_rateLimitPerSec;
    }
    m_lastRateRead = now;

    int32 rate = 0;
    if (m_pRedis)
    {
        std::string val;
        if (m_pRedis->HGet("cluster:config:cluster", "gate_rate_limit_per_sec", val) == REDIS_OK
            && !val.empty())
        {
            rate = (int32)atoi(val.c_str());
        }
    }
    if (rate != m_rateLimitPerSec)
    {
        m_rateLimitPerSec = rate;
        m_limiter.SetRate(rate);
        XLOG("[NetGateway] rate limit refreshed: %d msg/sec (%s)", rate,
             rate <= 0 ? "unlimited" : "enforced");
    }
    return m_rateLimitPerSec;
}

void CNetGateway::Kick(NetCID u64CID)
{
    std::shared_ptr<tcp_socket> sock;
    {
        std::lock_guard<std::mutex> lock(m_sockMutex);
        auto it = m_mapSockets.find(u64CID);
        if (it == m_mapSockets.end() || !it->second)
        {
            return;
        }
        sock = it->second;
    }
    auto executor = sock->get_executor();
    asio::post(executor,
               [sock]()
               {
                   asio::error_code ec;
                   sock->close(ec);
               });
}

void CNetGateway::KickConnection(NetCID u64CID)
{
    Kick(u64CID);
}
