/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: CNetBattleGateway.cpp
      Purpose:   战斗进程网络接入层实现：客户端直连 Battle 进程的握手与帧路由。

      实现 CNetBattleGateway 单例：Start 绑定 xNetAgent 回调，OnPackage 按 msgId=(BigType<<16)|SmallType 路由到 BattleActor
      HandleLogin 首帧校验 battle:auth/battle:roster 或观战名单后绑定 CID 会话，OnDisconnect 合成 DISCONNECT 通知投递 BattleActor
      SendToPlayer/SendToCID 由会话反查 CID 下发帧，GetRateLimitPerSec 每 5s 读 battle_rate_limit_per_sec 用令牌桶限流
  *****************************************************************/

#include "CNetBattleGateway.h"
#include "xNetAgent.h"
#include "xProtoParse.h"
#include "xActor/ActorMgr.h"
#include "xActor/Actor.h"
#include "xActor/ActorMsgId.h"
#include "xRedis/RedisMgr.h"
#include "xRedis/RedisCli.h"
#include "xLog/xLog.h"
#include "msg/msg_id.pb.h"
#include "msg/battle.pb.h"

#include <cstring>
#include <cstdlib>
#include <functional>

// =============================================================================
// 战斗进程直连帧 (与 Gate 同构): [uint32 u32Len][protobuf body]
//   - 首帧 = LOGIN_REQ (BATTLE_LOGIN_REQ): 网关在 C++ 层完成握手绑定会话。
//   - 后续帧按 msgId = (BigType<<16)|SmallType 投递给 BattleActor 的 on_msg,
//     body = [8 字节 playerId][protobuf body] (playerId 即发送方身份, 供回包)。
// =============================================================================

namespace
{
// 断线内部消息 SmallType (镜像 battle.lua BattleMsgId.DISCONNECT = 11, 不入 proto)
constexpr uint32 kBattleMsgDisconnect = 11;
// 观战登录内部消息 SmallType (镜像 battle.lua BattleMsgId.SPECTATOR_ENTER = 12, 不入 proto)
constexpr uint32 kBattleMsgSpectatorEnter = 12;
// 限流阈值配置键 (cluster:config:cluster Hash)
constexpr const char *kRateLimitField = "battle_rate_limit_per_sec";
// 限流阈值缓存 TTL (秒)
constexpr double kRateReadIntervalSec = 5.0;
}

CNetBattleGateway::CNetBattleGateway()
{
}

CNetBattleGateway::~CNetBattleGateway()
{
}

bool CNetBattleGateway::Start(uint16 u16Port)
{
    // 取得本进程 Redis 连接 (actor_cache 池): 读 battle:auth/roster + 限流阈值。
    // battle.lua 的 xEngine.Redis 同用此池, 保证与战斗花名册数据同源。
    m_pRedis = CRedisPoolMgr::getMe().GetRedisCli("actor_cache");
    if (!m_pRedis)
    {
        XWRN("[NetBattleGateway] no Redis client for pool 'actor_cache' — login handshake will reject");
    }

    m_stConfig.strServiceName = "BattleNet";
    m_stConfig.strHost = "0.0.0.0";
    m_stConfig.u16Port = u16Port;
    m_stConfig.u16CacheBuffSize = 32 * 1024;
    m_stConfig.pParam = this;

    m_stConfig.pAcceptCall
        = std::bind(&CNetBattleGateway::OnAccept, this, std::placeholders::_1, std::placeholders::_2);
    m_stConfig.pPackageCall
        = std::bind(&CNetBattleGateway::OnPackage, this, std::placeholders::_1, std::placeholders::_2,
                    std::placeholders::_3, std::placeholders::_4);
    m_stConfig.pDisconnect
        = std::bind(&CNetBattleGateway::OnDisconnect, this, std::placeholders::_1, std::placeholders::_2);
    m_stConfig.pSocketReady = std::bind(&CNetBattleGateway::OnSocketReady, this, std::placeholders::_1,
                                        std::placeholders::_2, std::placeholders::_3);

    m_pAgent = std::make_unique<xNetAgent>(m_stConfig);
    if (!m_pAgent->thread_start())
    {
        XERR("[NetBattleGateway] failed to start network thread");
        m_pAgent.reset();
        return false;
    }

    XLOG("[NetBattleGateway] listening on 0.0.0.0:%u", u16Port);
    return true;
}

void CNetBattleGateway::Stop()
{
    XLOG("[NetBattleGateway] stop requested (no-op — detached thread)");
}

NetCID CNetBattleGateway::OnAccept(void *, tcp_socket &)
{
    return m_nextCID.fetch_add(1);
}

void CNetBattleGateway::OnSocketReady(void *, NetCID u64CID, std::shared_ptr<tcp_socket> pSocket)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (pSocket)
    {
        m_mapSockets[u64CID] = pSocket;
        XDBG("[NetBattleGateway] socket registered cid=%lld", u64CID);
    }
    else
    {
        m_mapSockets.erase(u64CID);
        XDBG("[NetBattleGateway] socket unregistered cid=%lld", u64CID);
    }
}

void CNetBattleGateway::OnPackage(void *, tcp_socket &, uint64 u64CID, NetPackageSharedPtr pPackage)
{
    if (!pPackage || !pPackage->pPackageData || pPackage->u32Size < ClientFrameHeadSize)
    {
        return;
    }

    // 单连接消息接收限流: 先刷新阈值 (5s TTL 缓存读 Redis), 超限直接断开连接
    GetRateLimitPerSec();
    if (!m_limiter.Consume(u64CID))
    {
        XWRN("[NetBattleGateway] rate limit exceeded cid=%lld — disconnect", u64CID);
        Kick(u64CID);
        return;
    }

    // 帧: [uint32 u32Len][protobuf body]
    const uint8 *pData = pPackage->pPackageData;
    uint32 u32BodyLen = 0;
    std::memcpy(&u32BodyLen, pData, ClientFrameHeadSize);
    if (u32BodyLen == 0 || u32BodyLen > pPackage->u32Size - ClientFrameHeadSize)
    {
        XWRN("[NetBattleGateway] cid=%lld bad frame len=%u", u64CID, u32BodyLen);
        return;
    }
    std::string strBody(reinterpret_cast<const char *>(pData + ClientFrameHeadSize), u32BodyLen);

    // 会话未建立 → 首帧必须是 LOGIN_REQ
    bool loggedIn = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_mapSessions.find(u64CID);
        loggedIn = (it != m_mapSessions.end() && it->second.loggedIn);
    }
    if (!loggedIn)
    {
        HandleLogin(u64CID, strBody);
        return;
    }

    // 已登录 → 路由到 BattleActor
    int64 battleId = 0;
    int64 playerId = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_mapSessions.find(u64CID);
        if (it == m_mapSessions.end())
        {
            XWRN("[NetBattleGateway] cid=%lld session missing — drop", u64CID);
            return;
        }
        battleId = it->second.battleId;
        playerId = it->second.playerId;
    }

    uint32 bigType = 0;
    uint32 smallType = 0;
    netproto::ParseRouteFields(reinterpret_cast<const uint8 *>(strBody.data()), strBody.size(),
                               bigType, smallType);
    uint32 u32MsgId = (bigType << 16) | (smallType & 0xFFFF);

    CActor *pActor = CActorMgr::instance()->Find(EActorType_ABattle, battleId);
    if (!pActor)
    {
        // 战斗房已结算卸载, 仍有在途帧属正常清理窗口
        XWRN("[NetBattleGateway] cid=%lld battle=%lld uninstalled — drop small=%u", u64CID,
             (int64)battleId, smallType);
        return;
    }

    // 回包需要 playerId, 以小端 8 字节前置到 protobuf body (与 Gate 约定一致)
    std::string routedBody;
    routedBody.reserve(sizeof(int64) + strBody.size());
    uint8 idBytes[8];
    uint64 id = (uint64)playerId;
    for (int32 i = 0; i < 8; ++i)
    {
        idBytes[i] = (uint8)(id >> (8 * i));
    }
    routedBody.append(reinterpret_cast<const char *>(idBytes), 8);
    routedBody.append(strBody);

    pActor->PostMessage(u32MsgId, routedBody);
    XDBG("[NetBattleGateway] cid=%lld player=%lld msg=%u (small=%u) -> BattleActor battle=%lld",
         u64CID, (int64)playerId, u32MsgId, smallType, (int64)battleId);
}

void CNetBattleGateway::OnDisconnect(void *, NetCID u64CID)
{
    int64 battleId = 0;
    int64 playerId = 0;
    bool loggedIn = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_mapSessions.find(u64CID);
        if (it != m_mapSessions.end() && it->second.loggedIn)
        {
            battleId = it->second.battleId;
            playerId = it->second.playerId;
            loggedIn = true;
            // 反查映射仅在本 CID 时删除 (玩家可能已重连到新连接)
            auto rIt = m_mapPlayerCid.find(playerId);
            if (rIt != m_mapPlayerCid.end() && rIt->second == u64CID)
            {
                m_mapPlayerCid.erase(rIt);
            }
        }
        m_mapSessions.erase(u64CID);
    }
    m_limiter.Reset(u64CID);
    XLOG("[NetBattleGateway] client disconnect cid=%lld player=%lld battle=%lld", u64CID,
         (int64)playerId, (int64)battleId);

    if (!loggedIn)
    {
        return;
    }

    // 合成 DISCONNECT 通知投递给 BattleActor: msgId=(203<<16)|11, body=[8 字节 playerId]。
    // battle.lua 的 on_msg 收到 smallType==DISCONNECT 后走 handle_disconnect 标记离线。
    CActor *pActor = CActorMgr::instance()->Find(EActorType_ABattle, battleId);
    if (!pActor)
    {
        XWRN("[NetBattleGateway] disconnect cid=%lld — battle=%lld not installed", u64CID,
             (int64)battleId);
        return;
    }
    uint8 idBytes[8] = {0};
    uint64 id = (uint64)playerId;
    for (int32 i = 0; i < 8; ++i)
    {
        idBytes[i] = (uint8)(id >> (8 * i));
    }
    std::string body(reinterpret_cast<const char *>(idBytes), 8);
    pActor->PostMessage(((uint32)EActorType_ABattle << 16) | kBattleMsgDisconnect, body);
}

void CNetBattleGateway::HandleLogin(NetCID u64CID, const std::string &strBody)
{
    PbBattle::PBBattleLoginReq req;
    if (!req.ParseFromString(strBody) || req.battle_id() <= 0 || req.player_id() <= 0)
    {
        XWRN("[NetBattleGateway] cid=%lld login: bad LOGIN_REQ", u64CID);
        Kick(u64CID);
        return;
    }

    int64 battleId = req.battle_id();
    int64 playerId = req.player_id();
    bool spectate = req.spectate();

    // 校验: 观战登录验 battle:spectator:{battleId} 名单; 参战登录验 auth key + battle:roster 花名册
    bool ok = false;
    if (m_pRedis)
    {
        if (spectate)
        {
            std::string spectators;
            m_pRedis->Get("battle:spectator:" + std::to_string(battleId), spectators);
            ok = SpectatorContains(spectators, playerId);
            if (!ok)
            {
                XWRN("[NetBattleGateway] cid=%lld login: spectator reject battle=%lld player=%lld",
                     u64CID, (int64)battleId, (int64)playerId);
            }
        }
        else
        {
            std::string auth;
            std::string roster;
            m_pRedis->Get("battle:auth:" + std::to_string(battleId), auth);
            m_pRedis->Get("battle:roster:" + std::to_string(battleId), roster);
            ok = (!auth.empty() && auth == req.auth_key()) && RosterContains(roster, playerId);
            if (!ok)
            {
                XWRN("[NetBattleGateway] cid=%lld login: auth/roster reject battle=%lld player=%lld",
                     u64CID, (int64)battleId, (int64)playerId);
            }
        }
    }
    else
    {
        XWRN("[NetBattleGateway] cid=%lld login: redis unavailable — reject", u64CID);
    }

    if (!ok)
    {
        // 回错误并断开
        PbBattle::PBBattleLoginRsp rsp;
        rsp.set_big_type(PbID::AT_BATTLE);
        rsp.set_small_type(PbBattle::BATTLE_LOGIN_RSP);
        rsp.set_error_code(PbID::ERR_SESSION_INVALID);
        rsp.set_battle_id(battleId);
        std::string out;
        rsp.SerializeToString(&out);
        SendToCID(u64CID, out);
        Kick(u64CID);
        return;
    }

    // 绑定会话
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        Session s;
        s.battleId = battleId;
        s.playerId = playerId;
        s.loggedIn = true;
        s.spectate = spectate;
        m_mapSessions[u64CID] = s;
        m_mapPlayerCid[playerId] = u64CID;
    }

    // 观战登录: 通知 BattleActor 下推快照对齐 (观战帧流与参战者共用同一套 203 消息直发)
    if (spectate)
    {
        CActor *pActor = CActorMgr::instance()->Find(EActorType_ABattle, battleId);
        if (pActor)
        {
            uint8 idBytes[8] = {0};
            uint64 id = (uint64)playerId;
            for (int32 i = 0; i < 8; ++i)
            {
                idBytes[i] = (uint8)(id >> (8 * i));
            }
            std::string body(reinterpret_cast<const char *>(idBytes), 8);
            pActor->PostMessage(((uint32)EActorType_ABattle << 16) | kBattleMsgSpectatorEnter, body);
        }
        else
        {
            XWRN("[NetBattleGateway] cid=%lld spectator login — battle=%lld not installed", u64CID,
                 (int64)battleId);
        }
    }

    PbBattle::PBBattleLoginRsp rsp;
    rsp.set_big_type(PbID::AT_BATTLE);
    rsp.set_small_type(PbBattle::BATTLE_LOGIN_RSP);
    rsp.set_error_code(PbID::ERR_NONE);
    rsp.set_battle_id(battleId);
    std::string out;
    rsp.SerializeToString(&out);
    SendToCID(u64CID, out);
    XLOG("[NetBattleGateway] cid=%lld login ok battle=%lld player=%lld", u64CID, (int64)battleId,
         (int64)playerId);
}

bool CNetBattleGateway::RosterContains(const std::string &roster, int64 playerId)
{
    if (roster.empty())
    {
        return false;
    }
    // 花名册为逗号分隔的 playerId 列表; 前后补逗号后做子串匹配, 兼容首/尾元素。
    std::string haystack = "," + roster + ",";
    std::string needle = "," + std::to_string(playerId) + ",";
    return haystack.find(needle) != std::string::npos;
}

bool CNetBattleGateway::SpectatorContains(const std::string &spectators, int64 playerId)
{
    if (spectators.empty())
    {
        return false;
    }
    // 观战名单为逗号分隔的 playerId 列表 (SpectatorActor handle_enter 写入), 与 RosterContains 同构。
    std::string haystack = "," + spectators + ",";
    std::string needle = "," + std::to_string(playerId) + ",";
    return haystack.find(needle) != std::string::npos;
}

bool CNetBattleGateway::SendToPlayer(int64 playerId, const std::string &strBody)
{
    NetCID cid = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_mapPlayerCid.find(playerId);
        if (it == m_mapPlayerCid.end())
        {
            XDBG("[NetBattleGateway] SendToPlayer player=%lld no session", (int64)playerId);
            return false;
        }
        cid = it->second;
    }
    return SendToCID(cid, strBody);
}

bool CNetBattleGateway::SendToCID(NetCID u64CID, const std::string &strBody)
{
    std::shared_ptr<tcp_socket> sock;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_mapSockets.find(u64CID);
        if (it == m_mapSockets.end() || !it->second)
        {
            XWRN("[NetBattleGateway] SendToCID cid=%lld — no socket", u64CID);
            return false;
        }
        sock = it->second;
    }

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

    auto data = std::make_shared<std::string>(std::move(frame));
    auto executor = sock->get_executor();
    asio::post(executor,
               [sock, data]()
               {
                   asio::error_code ec;
                   asio::write(*sock, asio::buffer(*data), ec);
                   if (ec)
                   {
                       XERR("[NetBattleGateway] SendToCID write failed: %s", ec.message().c_str());
                   }
               });
    return true;
}

void CNetBattleGateway::Kick(NetCID u64CID)
{
    std::shared_ptr<tcp_socket> sock;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
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

int32 CNetBattleGateway::GetRateLimitPerSec()
{
    std::lock_guard<std::mutex> lock(m_rateMutex);
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<double>(now - m_lastRateRead).count() < kRateReadIntervalSec)
    {
        return m_rateLimitPerSec;
    }
    m_lastRateRead = now;

    int32 rate = 0;
    if (m_pRedis)
    {
        std::string val;
        if (m_pRedis->HGet("cluster:config:cluster", kRateLimitField, val) == REDIS_OK && !val.empty())
        {
            rate = (int32)atoi(val.c_str());
        }
    }
    if (rate != m_rateLimitPerSec)
    {
        m_rateLimitPerSec = rate;
        m_limiter.SetRate(rate);
        XLOG("[NetBattleGateway] rate limit refreshed: %d msg/sec (%s)", rate,
             rate <= 0 ? "unlimited" : "enforced");
    }
    return m_rateLimitPerSec;
}
