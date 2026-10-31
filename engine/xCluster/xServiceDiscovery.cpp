/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: xServiceDiscovery.cpp
      Purpose:   基于 Redis 的服务发现与注册实现：注册、心跳、发现与身份解析。

      实现 CServiceDiscovery 单例：Register/Heartbeat 写 cluster:process:{type}:{id} Hash 与类型索引 Set 并续 30s TTL
      DiscoverPeers/GetProcessInfo 读 Hash 并做 15s 心跳过滤，GetActorProcessId 读 cluster:actor:{type}:{id} 的 pid 定位 Actor 进程
      GetLinkRules 读 cluster:link:rules，WriteProcessInfo/ReadProcessInfo 用 HMSet/HGetAll 批量读写，GenerateProcessId 走 Redis INCR
  *****************************************************************/

#include "xServiceDiscovery.h"
#include "xProcessMetrics.h"
#include "xRedis/RedisMgr.h"
#include "xRedis/RedisCli.h"
#include "xLog/xLog.h"
#include <ctime>

namespace
{
// 心跳超时阈值: 超过该秒数未心跳即视为进程已死 (心跳间隔 5s, 允许丢失 3 拍)。
// DiscoverPeers 与 IsProcessRunning 共用, 避免僵尸进程被当作存活节点反复连接。
constexpr int64 kStaleHeartbeatSeconds = 15;

// 进程注册 TTL (秒): 本进程注册 Hash 与类型索引 Set 的过期时间。进程每次心跳 (5s)
// 用 Expire 续期; 一旦停止心跳, Redis 在 TTL 到期后自动删除僵尸数据, 无需其它进程
// 主动扫描清理 (优雅注销 Deregister 在 while(true) 主循环下不可达, 全依赖此 TTL 兜底)。
// 取值 30s = 6 拍, 大于心跳过滤阈值 (15s), 保证「短暂卡顿但尚未判定死亡」的进程其
// Hash 不会先于发现过滤被提前删掉。
constexpr int64 kProcessRegTtlSeconds = 30;
} // namespace

bool CServiceDiscovery::Init(const std::string &redisPoolTitle)
{
    m_strRedisPoolTitle = redisPoolTitle;

    // 从池中 checkout 一条专用连接并长期持有 (进程生命周期内复用)。
    // 注意: GetRedisCli 是「取出」语义, 本模块持有此连接、不再归还,
    // 因此只有这一条连接被占用, 不会随每次心跳/发现反复新建。
    m_pCli = CRedisPoolMgr::getMe().GetRedisCli(m_strRedisPoolTitle);
    if (!m_pCli)
    {
        XWRN("[ServiceDiscovery] Redis pool '%s' not available — running in standalone mode",
             m_strRedisPoolTitle.c_str());
        m_bRedisAvailable = false;
        return false;
    }
    m_bRedisAvailable = true;
    XLOG("[ServiceDiscovery] Initialized with Redis pool '%s' (dedicated connection)",
         m_strRedisPoolTitle.c_str());
    return true;
}

// =============================================================================
// 注册
// =============================================================================
bool CServiceDiscovery::Register(const ProcessInfo &info)
{
    m_stSelfInfo = info;
    if (!m_bRedisAvailable)
    {
        XLOG("[ServiceDiscovery] Register skipped — Redis unavailable (using local fallback)");
        return true; // 非致命: standalone 模式
    }
    if (!WriteProcessInfo(info))
    {
        XERR("[ServiceDiscovery] Failed to register process %s", info.RedisKey().c_str());
        return false;
    }
    // 加入类型索引集合
    RedisSAdd("cluster:process:by_type:" + std::to_string((int32)info.type),
              std::to_string(info.processId));
    // 注册即设 TTL: 本进程注册 Hash 与类型索引 Set 各续期一次, 后续由心跳持续续期。
    // 进程停止心跳后由 Redis 按 TTL 自动删除僵尸数据, 不主动扫描清理。
    RedisExpire(info.RedisKey(), kProcessRegTtlSeconds);
    RedisExpire("cluster:process:by_type:" + std::to_string((int32)info.type),
                kProcessRegTtlSeconds);
    XCLOG("Registered: %s type=%d name=%s host=%s:%u", info.RedisKey().c_str(), (int32)info.type,
          info.processName.c_str(), info.host.c_str(), info.rpcPort);
    return true;
}

// =============================================================================
// ConfirmNodeDetail — 写入完整节点详情并读回比对
//
// 容器节点初始化完成的标志: Redis 中的 ip/port/authkey 与本进程一致。
// 读回比对用于发现 Redis 侧的写入偏差 (如多实例竞争同一 key)。
// =============================================================================
bool CServiceDiscovery::ConfirmNodeDetail(const ProcessInfo &info)
{
    if (!m_bRedisAvailable)
    {
        XLOG("[ServiceDiscovery] ConfirmNodeDetail skipped — Redis unavailable");
        return false;
    }

    // 先全量写入 (含 ip/authkey), 再读回关键字段比对
    if (!WriteProcessInfo(info))
    {
        XERR("[ServiceDiscovery] ConfirmNodeDetail: write failed for %s", info.RedisKey().c_str());
        return false;
    }

    std::string key = info.RedisKey();
    std::string gotIp = RedisHGet(key, "ip");
    std::string gotPort = RedisHGet(key, "port");
    std::string gotKey = RedisHGet(key, "authkey");

    bool ok = (gotIp == info.ip) && (gotPort == std::to_string(info.rpcPort))
              && (gotKey == info.authKey);
    if (!ok)
    {
        XERR("[ServiceDiscovery] ConfirmNodeDetail mismatch: ip=%s/%s port=%s/%u auth=%s/%s",
             gotIp.c_str(), info.ip.c_str(), gotPort.c_str(), info.rpcPort, gotKey.c_str(),
             info.authKey.c_str());
        return false;
    }

    XCLOG("Node detail confirmed: %s ip=%s port=%u auth=%s", key.c_str(), info.ip.c_str(),
          info.rpcPort, info.authKey.c_str());
    return true;
}

// =============================================================================
// 心跳
// =============================================================================
bool CServiceDiscovery::Heartbeat()
{
    if (!m_bRedisAvailable)
    {
        return true;
    }
    int64 now = (int64)time(nullptr);
    m_stSelfInfo.lastHeartbeat = now;
    const std::string &key = m_stSelfInfo.RedisKey();
    // 自愈: 心跳每拍重写完整进程信息 (含 name/ip/port/authkey/id/type/host 等身份字段),
    // 而非只写 heartbeat/state。注册 Hash 一旦因 TTL 过期 / Redis 重启 / 手动清理被删,
    // 后续心跳若只重建运行时字段, 身份字段会永久丢失 → WebPlatform 节点显示 name 缺失
    // (回退 "process-<id>"), 且 NodeManager::FindChildVersion 读不到 name 致灰度判断静默跳过。
    bool ok = WriteProcessInfo(m_stSelfInfo);
    // 自愈索引: 每拍重新 SAdd 本进程 ID 到类型集合 (SAdd 幂等, 开销可忽略), 防止索引
    // 成员在边缘场景下丢失。
    RedisSAdd("cluster:process:by_type:" + std::to_string((int32)m_stSelfInfo.type),
              std::to_string(m_stSelfInfo.processId));
    // TTL 续期: 每拍刷新本进程注册 Hash 与类型索引 Set 的过期时间。进程停止心跳后,
    // 由 Redis 按 TTL 自动删除僵尸数据, 不主动扫描清理。
    RedisExpire(key, kProcessRegTtlSeconds);
    RedisExpire("cluster:process:by_type:" + std::to_string((int32)m_stSelfInfo.type),
                kProcessRegTtlSeconds);
    return ok;
}

// =============================================================================
// 更新负载
// =============================================================================
bool CServiceDiscovery::UpdateLoad(int32 loadScore)
{
    m_stSelfInfo.loadScore = loadScore;
    if (!m_bRedisAvailable)
    {
        return true;
    }
    return RedisHSet(m_stSelfInfo.RedisKey(), "load", std::to_string(loadScore));
}

// =============================================================================
// 即时上报负载 (仅写 Redis, 不动 m_stSelfInfo)
// =============================================================================
bool CServiceDiscovery::FlushLoad(int32 loadScore)
{
    if (!m_bRedisAvailable)
    {
        return true;
    }
    return RedisHSet(m_stSelfInfo.RedisKey(), "load", std::to_string(loadScore));
}

// =============================================================================
// 更新资源占用 — 心跳时采样本进程 CPU/内存, 写入 Redis 供负载均衡与 WebPlatform 读取
// =============================================================================
bool CServiceDiscovery::UpdateResource(int32 cpuPercent, int64 memRssMb, int32 memPercent)
{
    m_stSelfInfo.cpuPercent = cpuPercent;
    m_stSelfInfo.memRssMb = memRssMb;
    m_stSelfInfo.memPercent = memPercent;
    if (!m_bRedisAvailable)
    {
        return true;
    }
    const std::string &key = m_stSelfInfo.RedisKey();
    return RedisHSet(key, "cpu_percent", std::to_string(cpuPercent))
           && RedisHSet(key, "mem_rss_mb", std::to_string(memRssMb))
           && RedisHSet(key, "mem_percent", std::to_string(memPercent));
}

// =============================================================================
// 更新状态
// =============================================================================
bool CServiceDiscovery::UpdateState(EProcessState newState)
{
    m_stSelfInfo.state = newState;
    if (!m_bRedisAvailable)
    {
        return true;
    }
    return RedisHSet(m_stSelfInfo.RedisKey(), "state", std::to_string((int32)newState));
}

// =============================================================================
// 更新排空标志
// =============================================================================
bool CServiceDiscovery::UpdateDrain(bool draining)
{
    m_stSelfInfo.draining = draining;
    if (!m_bRedisAvailable)
    {
        return true;
    }
    return RedisHSet(m_stSelfInfo.RedisKey(), "drain", draining ? "1" : "0");
}

// =============================================================================
// 版本 / 内容版本上报 (P9 灰度更新)
// =============================================================================
bool CServiceDiscovery::UpdateVersion(const std::string &version, const std::string &contentVersion)
{
    m_stSelfInfo.version = version;
    m_stSelfInfo.contentVersion = contentVersion;
    if (!m_bRedisAvailable)
    {
        return true;
    }
    const std::string &key = m_stSelfInfo.RedisKey();
    return RedisHSet(key, "version", version) && RedisHSet(key, "content_version", contentVersion);
}

std::string CServiceDiscovery::GetContentUpdateConfig(const std::string &field)
{
    if (!m_bRedisAvailable)
    {
        return "";
    }
    return RedisHGet("cluster:update:content:config", field);
}

std::string CServiceDiscovery::GetConfig(const std::string &field)
{
    if (!m_bRedisAvailable)
    {
        return "";
    }
    return RedisHGet("cluster:config:cluster", field);
}

// =============================================================================
// 注销
// =============================================================================
bool CServiceDiscovery::Deregister()
{
    if (!m_bRedisAvailable)
    {
        return true;
    }
    UpdateState(EProcessState_Shutdown);
    RedisSRem("cluster:process:by_type:" + std::to_string((int32)m_stSelfInfo.type),
              std::to_string(m_stSelfInfo.processId));
    XCLOG("Deregistered: %s", m_stSelfInfo.RedisKey().c_str());
    return true;
}

// =============================================================================
// 发现对等进程
// =============================================================================
std::vector<ProcessInfo> CServiceDiscovery::DiscoverPeers(EProcessType targetType)
{
    std::vector<ProcessInfo> result;
    if (!m_bRedisAvailable)
    {
        return result;
    }

    int64 now = (int64)time(nullptr);
    auto members = RedisSMembers("cluster:process:by_type:" + std::to_string((int32)targetType));
    for (auto &idStr : members)
    {
        std::string key = "cluster:process:" + std::to_string((int32)targetType) + ":" + idStr;
        ProcessInfo info = ReadProcessInfo(key);
        if (info.processId == 0)
        {
            continue;
        }

        // 心跳过滤: 跳过已死但未注销的僵尸进程 (state 停留在 Running, 但心跳已停)。
        // 否则 DiscoverAndConnect 会反复尝试连接死节点, 阻塞 RPC io 线程导致链路延迟。
        if (now - info.lastHeartbeat > kStaleHeartbeatSeconds)
        {
            XCWRN("Skip stale node: %s:%lld (heartbeat %llds ago)", ProcessTypeName(info.type),
                  (int64)info.processId, (int64)(now - info.lastHeartbeat));
            continue;
        }
        result.push_back(info);
    }
    return result;
}

std::vector<ProcessInfo> CServiceDiscovery::GetAllRunningPeers()
{
    std::vector<ProcessInfo> result;
    if (!m_bRedisAvailable)
    {
        return result;
    }

    static const EProcessType kProcessTypes[]
        = {EProcessType_PGate, EProcessType_PLogin, EProcessType_PGlobal, EProcessType_PGame,
           EProcessType_PBattle};
    for (EProcessType t : kProcessTypes)
    {
        auto peers = DiscoverPeers(t);
        for (auto &p : peers)
        {
            if (IsProcessRunning(p.type, p.processId))
            {
                result.push_back(p);
            }
        }
    }
    return result;
}

ProcessInfo CServiceDiscovery::GetProcessInfo(EProcessType type, int64 processId)
{
    std::string key
        = "cluster:process:" + std::to_string((int32)type) + ":" + std::to_string(processId);
    return ReadProcessInfo(key);
}

bool CServiceDiscovery::IsProcessRunning(EProcessType type, int64 processId) const
{
    std::string key
        = "cluster:process:" + std::to_string((int32)type) + ":" + std::to_string(processId);
    std::lock_guard<std::mutex> lock(m_redisMutex);
    auto *pCli = m_pCli;
    if (!pCli)
    {
        return false;
    }
    // 检查心跳是否在阈值内 (kStaleHeartbeatSeconds)
    RecordDataST data;
    std::string hbKey = key;
    if (pCli->HGet(key, "heartbeat", &data) == REDIS_OK)
    {
        int64 hb = strtoll(std::string(data.Data(), data.Size()).c_str(), nullptr, 10);
        int64 now = (int64)time(nullptr);
        return (now - hb) <= kStaleHeartbeatSeconds;
    }
    return false;
}

bool CServiceDiscovery::IsHeartbeatStale(const ProcessInfo &info) const
{
    // 心跳超时判定: 与 DiscoverPeers / IsProcessRunning 共用 kStaleHeartbeatSeconds 阈值。
    // 用于 CLinkManager 在选择/刷新对端时过滤「心跳已停但 state 仍为 Running」的僵尸进程。
    int64 now = (int64)time(nullptr);
    return (now - info.lastHeartbeat) > kStaleHeartbeatSeconds;
}

// =============================================================================
// Actor 身份解析 — 读 cluster:actor:{type}:{id} 的 pid 字段 (存的是集群 processId)
// =============================================================================
int64 CServiceDiscovery::GetActorProcessId(EActorType type, int64 actorId)
{
    if (!m_bRedisAvailable || actorId == 0)
    {
        return 0;
    }
    std::string key
        = "cluster:actor:" + std::to_string((int32)type) + ":" + std::to_string(actorId);
    std::string pidStr = RedisHGet(key, "pid");
    if (pidStr.empty())
    {
        return 0;
    }
    return atoll(pidStr.c_str());
}

// =============================================================================
// 链接规则
// =============================================================================
std::map<EProcessType, std::vector<EProcessType>> CServiceDiscovery::GetLinkRules()
{
    std::map<EProcessType, std::vector<EProcessType>> rules;
    if (!m_bRedisAvailable)
    {
        // 默认规则: 所有类型互联
        std::vector<EProcessType> all
            = {EProcessType_PGate, EProcessType_PLogin, EProcessType_PGlobal, EProcessType_PGame,
               EProcessType_PBattle};
        for (auto &t : all)
        {
            rules[t] = all;
        }
        return rules;
    }
    // 从 Redis Hash 读取
    static const EProcessType kProcessTypes[]
        = {EProcessType_PGate, EProcessType_PLogin, EProcessType_PGlobal, EProcessType_PGame,
           EProcessType_PBattle};
    for (EProcessType t : kProcessTypes)
    {
        std::string val = RedisHGet("cluster:link:rules", std::to_string((int32)t));
        if (!val.empty())
        {
            std::vector<EProcessType> targets;
            // 逗号分隔
            size_t pos = 0;
            while (pos < val.size())
            {
                size_t comma = val.find(',', pos);
                std::string tok = val.substr(pos, comma - pos);
                int32 tt = atoi(tok.c_str());
                if (tt >= 100)
                {
                    targets.push_back((EProcessType)tt);
                }
                if (comma == std::string::npos)
                {
                    break;
                }
                pos = comma + 1;
            }
            rules[t] = targets;
        }
    }
    return rules;
}

bool CServiceDiscovery::SetLinkRule(EProcessType fromType, const std::vector<EProcessType> &toTypes)
{
    if (!m_bRedisAvailable)
    {
        return false;
    }
    std::string val;
    for (size_t i = 0; i < toTypes.size(); ++i)
    {
        if (i > 0)
        {
            val += ",";
        }
        val += std::to_string((int32)toTypes[i]);
    }
    return RedisHSet("cluster:link:rules", std::to_string((int32)fromType), val);
}

// =============================================================================
// ID 生成
// =============================================================================
int64 CServiceDiscovery::GenerateProcessId(EProcessType type)
{
    if (!m_bRedisAvailable)
    {
        return -1;
    }
    // 全局唯一序列: 对等连接表/RPC 客户端以 processId 为主键, 必须跨类型唯一
    return RedisIncr("cluster:process:id_seq");
}

// =============================================================================
// Redis 辅助方法
// =============================================================================
CRedisCli *CServiceDiscovery::GetRedisCli()
{
    return m_pCli;
}

bool CServiceDiscovery::WriteProcessInfo(const ProcessInfo &info)
{
    std::lock_guard<std::mutex> lock(m_redisMutex);
    auto *pCli = GetRedisCli();
    if (!pCli)
    {
        return false;
    }

    // 批量写: 一次 HMSet 替代逐字段 HSet (心跳回写路径, 19 字段 → 1 次往返)。
    std::map<std::string, std::string> fields;
    for (const auto &metric : xProcessMetrics::kProcessMetrics)
    {
        fields[metric.field] = metric.get(info);
    }
    return pCli->HMSet(info.RedisKey(), fields) == REDIS_OK;
}

ProcessInfo CServiceDiscovery::ReadProcessInfo(const std::string &redisKey)
{
    ProcessInfo info;
    std::lock_guard<std::mutex> lock(m_redisMutex);
    auto *pCli = GetRedisCli();
    if (!pCli)
    {
        return info;
    }

    // 批量读: 一次 HGetAll 替代逐字段 HGet。此函数是 GetBestPeer 的每登录热路径
    // (选节点 × 候选数), 原 19 次逐字段 HGet 的往返是 Gate 登录转发串行点的主因。
    std::map<std::string, std::string> fields;
    if (pCli->HGetAll(redisKey, fields) != REDIS_OK)
    {
        return info;
    }

    // 逐字段反序列化; 缺失字段回填空串 (数值归 0), 与原逐字段 hget 语义一致。
    for (const auto &metric : xProcessMetrics::kProcessMetrics)
    {
        auto it = fields.find(metric.field);
        std::string val = (it != fields.end()) ? it->second : std::string();
        metric.set(info, val);
    }
    return info;
}

bool CServiceDiscovery::RedisHSet(const std::string &key, const std::string &field,
                                  const std::string &value)
{
    std::lock_guard<std::mutex> lock(m_redisMutex);
    auto *pCli = GetRedisCli();
    if (!pCli)
    {
        return false;
    }
    RecordDataST d;
    d.Resize((int32)value.size());
    memcpy(d.MutableData(), value.data(), value.size());
    d.SetSize((int32)value.size());
    return pCli->HSet(key, field, &d) == REDIS_OK;
}

std::string CServiceDiscovery::RedisHGet(const std::string &key, const std::string &field)
{
    std::lock_guard<std::mutex> lock(m_redisMutex);
    auto *pCli = GetRedisCli();
    if (!pCli)
    {
        return "";
    }
    RecordDataST d;
    if (pCli->HGet(key, field, &d) == REDIS_OK && d.Size() > 0)
    {
        return std::string(d.Data(), d.Size());
    }
    return "";
}

bool CServiceDiscovery::RedisSAdd(const std::string &key, const std::string &member)
{
    std::lock_guard<std::mutex> lock(m_redisMutex);
    auto *pCli = GetRedisCli();
    if (!pCli)
    {
        return false;
    }
    return pCli->SAdd(key, member) == REDIS_OK;
}

bool CServiceDiscovery::RedisSRem(const std::string &key, const std::string &member)
{
    std::lock_guard<std::mutex> lock(m_redisMutex);
    auto *pCli = GetRedisCli();
    if (!pCli)
    {
        return false;
    }
    return pCli->SRem(key, member) == REDIS_OK;
}

std::vector<std::string> CServiceDiscovery::RedisSMembers(const std::string &key)
{
    std::vector<std::string> result;
    std::lock_guard<std::mutex> lock(m_redisMutex);
    auto *pCli = GetRedisCli();
    if (!pCli)
    {
        return result;
    }
    pCli->SMembers(key, result);
    return result;
}

int64 CServiceDiscovery::RedisIncr(const std::string &key)
{
    std::lock_guard<std::mutex> lock(m_redisMutex);
    auto *pCli = GetRedisCli();
    if (!pCli)
    {
        return -1;
    }
    return pCli->Incr(key);
}

bool CServiceDiscovery::RedisSet(const std::string &key, const std::string &value)
{
    std::lock_guard<std::mutex> lock(m_redisMutex);
    auto *pCli = GetRedisCli();
    if (!pCli)
    {
        return false;
    }
    RecordDataST d;
    d.Resize((int32)value.size());
    memcpy(d.MutableData(), value.data(), value.size());
    d.SetSize((int32)value.size());
    return pCli->Set(key, &d) == REDIS_OK;
}

std::string CServiceDiscovery::RedisGet(const std::string &key)
{
    std::lock_guard<std::mutex> lock(m_redisMutex);
    auto *pCli = GetRedisCli();
    if (!pCli)
    {
        return "";
    }
    RecordDataST d;
    if (pCli->Get(key, &d) == REDIS_OK && d.Size() > 0)
    {
        return std::string(d.Data(), d.Size());
    }
    return "";
}

bool CServiceDiscovery::RedisExpire(const std::string &key, int64 ttlSec)
{
    std::lock_guard<std::mutex> lock(m_redisMutex);
    auto *pCli = GetRedisCli();
    if (!pCli)
    {
        return false;
    }
    return pCli->Expire(key, ttlSec) == REDIS_OK;
}
