#include "Actor.h"
#include "xBase/xXMLParser.h"
#include "xLog/xLog.h"
#include "xTrace/CTraceManager.h"
#include "xRedis/RedisCli.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace
{
const char *kRecoveryLogKey = "actor:recovery:log";
const int32 kRecoveryLogMax = 2000; // 恢复日志封顶条数

// 恢复快照 actor:state:{type}:{id} 的 TTL (秒) 默认值: 24h。实际按 Actor 类型从
// cluster:config:actor-state-ttl 读取 (ResolveStateTtlSeconds), 缺省回退此值。
// 心跳续期 (RefreshStateTtl) / 脏字段落盘 (FlushState) / 干净卸载 (DeleteState) 均用该 TTL,
// 使快照在 Actor 移除后保留配置时长再过期 (支持删除后短暂可恢复), 到期由 Redis 过期清理。
constexpr int64 kActorStateDefaultTtlSeconds = 86400;

// 按 Actor 类型配置状态快照失效时长的 Redis Hash (field = 类型十进制串, value = 秒)
constexpr const char *kActorStateTtlCfgKey = "cluster:config:actor-state-ttl";

// 最小 JSON 字符串转义 (本仓库无 JSON 库; 上报结构里仅 name 为字符串字段)
std::string JsonEscape(const std::string &s)
{
    std::string out;
    out.reserve(s.size() + 2);
    for (unsigned char c : s)
    {
        switch (c)
        {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (c < 0x20)
            {
                char buf[8] = {0};
                snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
                out += buf;
            }
            else
            {
                out += (char)c;
            }
        }
    }
    return out;
}
} // namespace

// =============================================================================
// Actor 上报结构序列化 (监控上报模块)
// =============================================================================

std::string CActor::BuildReportJson(int64 nowSec) const
{
    // 单行 JSON, 字段与 Actor.h 的 ActorReport 一一对应; 增减字段同步此处。
    std::string json = std::string("{\"type\":") + std::to_string((int32)m_i32ActorType) + ",\"id\":"
           + std::to_string((int64)m_i64ActorID) + ",\"global_id\":"
           + std::to_string((uint64)m_i64GlobalId) + ",\"name\":\"" + JsonEscape(m_strActorName)
           + "\",\"load_score\":" + std::to_string((int32)m_i32LoadScore) + ",\"create_time\":"
           + std::to_string((int64)m_i64CreateTime) + ",\"state\":" + std::to_string((int32)m_i32State)
           + ",\"owner_id\":" + std::to_string((int64)m_i64OwnerID) + ",\"reported_at\":"
           + std::to_string((int64)nowSec);

    // 业务上报扩展 (如 BattleActor 的战斗核心状态): 作为 "battle" 子对象拼入, 供监控页展示
    std::string extra = GetReportExtraJson();
    if (!extra.empty())
    {
        json += ",\"battle\":" + extra;
    }
    json += "}";
    return json;
}

// =============================================================================
// 帧级脏读写 Redis 状态框架 (P7)
// =============================================================================

std::string CActor::StateRedisKey() const
{
    char buf[96] = {0};
    snprintf(buf, sizeof(buf), "actor:state:%d:%lld", (int32)m_i32ActorType, (int64)m_i64ActorID);
    return buf;
}

// 按 Actor 类型解析状态快照失效时长: 读 cluster:config:actor-state-ttl 的 {type} 字段,
// 缺失/空/非法/<=0 一律回退默认 86400。供 RefreshStateTtl / FlushState / DeleteState 共用。
int64 CActor::ResolveStateTtlSeconds(CRedisCli *cli) const
{
    if (!cli)
    {
        return kActorStateDefaultTtlSeconds;
    }
    std::string val;
    if (cli->HGet(kActorStateTtlCfgKey, std::to_string((int32)m_i32ActorType), val) != REDIS_OK
        || val.empty())
    {
        return kActorStateDefaultTtlSeconds;
    }
    char *end = nullptr;
    int64 ttl = strtoll(val.c_str(), &end, 10);
    if (end == val.c_str() || *end != '\0' || ttl <= 0)
    {
        return kActorStateDefaultTtlSeconds;
    }
    return ttl;
}

void CActor::SetStateField(const std::string &key, const std::string &value)
{
    std::lock_guard<std::mutex> lock(m_stStateMutex);
    m_mapState[key] = value; // 更新内存快照
    m_mapDirty[key] = value; // 标记脏字段 (待落盘)
}

bool CActor::GetStateField(const std::string &key, std::string &out) const
{
    std::lock_guard<std::mutex> lock(m_stStateMutex);
    auto it = m_mapState.find(key);
    if (it == m_mapState.end())
    {
        return false;
    }
    out = it->second;
    return true;
}

bool CActor::HasDirtyState() const
{
    std::lock_guard<std::mutex> lock(m_stStateMutex);
    return !m_mapDirty.empty();
}

bool CActor::FlushState(CRedisCli *cli)
{
    if (!cli || !m_bRecoveryEnabled)
    {
        return true; // 无客户端/未开启 → 视为成功(空操作)
    }

    // 取出并清空脏字段 (锁内 swap 出, 锁外落盘, 避免长时间持锁)
    std::map<std::string, std::string> dirty;
    {
        std::lock_guard<std::mutex> lock(m_stStateMutex);
        if (m_mapDirty.empty())
        {
            return true; // 无脏数据
        }
        dirty.swap(m_mapDirty);
    }

    std::string key = StateRedisKey();
    int32 okCount = 0;
    for (auto &kv : dirty)
    {
        if (cli->HSet(key, kv.first, kv.second) == REDIS_OK)
        {
            ++okCount;
        }
    }

    if (okCount > 0)
    {
        // 写成功即续期快照 TTL (与 RefreshStateTtl 兜底互补)
        cli->Expire(key, ResolveStateTtlSeconds(cli));
        LogRecovery(cli, "saved", std::to_string(okCount) + " fields");
    }
    else
    {
        XWRN("[Actor:%s] FlushState: %zu dirty fields failed to write", m_strActorName.c_str(),
             dirty.size());
        // 写失败的字段回填脏表, 下轮重试
        std::lock_guard<std::mutex> lock(m_stStateMutex);
        for (auto &kv : dirty)
        {
            m_mapDirty[kv.first] = kv.second;
        }
    }
    return okCount == (int32)dirty.size();
}

bool CActor::LoadState(CRedisCli *cli)
{
    if (!cli || !m_bRecoveryEnabled)
    {
        return false;
    }

    std::string key = StateRedisKey();
    std::map<std::string, std::string> snapshot;
    if (cli->HGetAll(key, snapshot) != REDIS_OK || snapshot.empty())
    {
        return false; // 无快照 (首次创建) 或读取失败
    }

    {
        std::lock_guard<std::mutex> lock(m_stStateMutex);
        for (auto &kv : snapshot)
        {
            m_mapState[kv.first] = kv.second; // 回填内存快照 (供 Lua GetStateField 读取)
        }
    }

    LogRecovery(cli, "recovered", std::to_string(snapshot.size()) + " fields restored");
    XLOG("[Actor:%s] LoadState recovered %zu field(s) from %s", m_strActorName.c_str(),
         snapshot.size(), key.c_str());
    return true;
}

bool CActor::RefreshStateTtl(CRedisCli *cli)
{
    if (!cli || !m_bRecoveryEnabled)
    {
        return true; // 无客户端/未开启恢复 → 空操作
    }
    return cli->Expire(StateRedisKey(), ResolveStateTtlSeconds(cli)) == REDIS_OK;
}

bool CActor::DeleteState(CRedisCli *cli)
{
    if (!cli || !m_bRecoveryEnabled)
    {
        return true; // 无客户端/未开启恢复 → 空操作
    }
    // 干净卸载不再立即 Del, 而是按类型配置的失效时长过期 (删除后短暂可恢复)
    return cli->Expire(StateRedisKey(), ResolveStateTtlSeconds(cli)) == REDIS_OK;
}

void CActor::LogRecovery(CRedisCli *cli, const char *event, const std::string &detail)
{
    if (!cli)
    {
        return;
    }
    // 格式与 WebPlatform actor_recovery.php 一致: {ts}|{type}|{id}|{event}|{detail}
    std::string line = std::to_string((int64)time(nullptr)) + "|"
                       + std::to_string((int32)m_i32ActorType) + "|"
                       + std::to_string((int64)m_i64ActorID) + "|" + event + "|" + detail;
    cli->RPush(kRecoveryLogKey, line);
    cli->LTrim(kRecoveryLogKey, -kRecoveryLogMax, -1);
}

void CActor::PostMessage(uint32 u32MsgId, const std::string &strBody)
{
    ActorMessage msg;
    msg.u32MsgId = u32MsgId;
    msg.body = std::make_shared<std::string>(strBody);
    msg.enqueuedAt = std::chrono::steady_clock::now();
    m_stMessageQueue.Push(std::move(msg));
}

void CActor::RequestStop()
{
    ActorMessage msg; // body 为空 = 停止信号
    m_stMessageQueue.Push(std::move(msg));
}

void CActor::BindResumeContext(asio::io_context &ioc)
{
    m_stMessageQueue.SetResumeContext(ioc);
}

ActorTask CActor::MessageLoop()
{
    XLOG("[Actor:%s] message loop started", GetActorName().c_str());
    for (;;)
    {
        ActorMessage msg = co_await m_stMessageQueue.Pop();
        if (!msg.body)
        {
            break; // 空 body = 停止信号
        }

        // 链路追踪: 上报 Actor 消息排队延迟 (Recv 方向, 仅慢消息)
        if (msg.enqueuedAt != std::chrono::steady_clock::time_point{})
        {
            int64 latencyMs = (int64)std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::steady_clock::now() - msg.enqueuedAt)
                                  .count();
            TraceEndpoint peer; // Actor 消息无显式对端, 传空
            CTraceManager::instance()->ReportProtocolLatency(TraceDir_Recv, latencyMs, msg.u32MsgId,
                                                             peer);
        }

        OnMessage(msg.u32MsgId, *msg.body);
    }
    XLOG("[Actor:%s] message loop stopped", GetActorName().c_str());
}
