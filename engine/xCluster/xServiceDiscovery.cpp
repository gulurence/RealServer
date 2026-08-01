#include "xServiceDiscovery.h"
#include "xRedis/RedisMgr.h"
#include "xRedis/RedisCli.h"
#include "xLog/xLog.h"
#include <ctime>

bool CServiceDiscovery::Init(const std::string& redisPoolTitle) {
    m_strRedisPoolTitle = redisPoolTitle;

    // 测试 Redis 是否可用
    auto* pCli = CRedisPoolMgr::getMe().GetRedisCli(m_strRedisPoolTitle);
    if (!pCli) {
        XWRN("[ServiceDiscovery] Redis pool '%s' not available — running in standalone mode",
             m_strRedisPoolTitle.c_str());
        m_bRedisAvailable = false;
        return false;
    }
    CRedisPoolMgr::getMe().GetRedisCli(m_strRedisPoolTitle); // release? 由池管理
    m_bRedisAvailable = true;
    XLOG("[ServiceDiscovery] Initialized with Redis pool '%s'", m_strRedisPoolTitle.c_str());
    return true;
}

// =============================================================================
// 注册
// =============================================================================
bool CServiceDiscovery::Register(const ProcessInfo& info) {
    m_stSelfInfo = info;
    if (!m_bRedisAvailable) {
        XLOG("[ServiceDiscovery] Register skipped — Redis unavailable (using local fallback)");
        return true;  // 非致命: standalone 模式
    }
    if (!WriteProcessInfo(info)) {
        XERR("[ServiceDiscovery] Failed to register process %s", info.RedisKey().c_str());
        return false;
    }
    // 加入类型索引集合
    RedisSAdd("cluster:process:by_type:" + std::to_string((int32)info.type),
              std::to_string(info.processId));
    XCLOG("Registered: %s type=%d name=%s host=%s:%u",
         info.RedisKey().c_str(), (int32)info.type, info.processName.c_str(),
         info.host.c_str(), info.grpcPort);
    return true;
}

// =============================================================================
// 心跳
// =============================================================================
bool CServiceDiscovery::Heartbeat() {
    if (!m_bRedisAvailable) return true;
    int64 now = (int64)time(nullptr);
    m_stSelfInfo.lastHeartbeat = now;
    return RedisHSet(m_stSelfInfo.RedisKey(), "heartbeat", std::to_string(now))
        && RedisHSet(m_stSelfInfo.RedisKey(), "state", std::to_string(m_stSelfInfo.state));
}

// =============================================================================
// 更新负载
// =============================================================================
bool CServiceDiscovery::UpdateLoad(int32 loadScore) {
    m_stSelfInfo.loadScore = loadScore;
    if (!m_bRedisAvailable) return true;
    return RedisHSet(m_stSelfInfo.RedisKey(), "load", std::to_string(loadScore));
}

// =============================================================================
// 更新状态
// =============================================================================
bool CServiceDiscovery::UpdateState(EProcessState newState) {
    m_stSelfInfo.state = newState;
    if (!m_bRedisAvailable) return true;
    return RedisHSet(m_stSelfInfo.RedisKey(), "state", std::to_string((int32)newState));
}

// =============================================================================
// 注销
// =============================================================================
bool CServiceDiscovery::Deregister() {
    if (!m_bRedisAvailable) return true;
    UpdateState(EProcessState_Shutdown);
    RedisSRem("cluster:process:by_type:" + std::to_string((int32)m_stSelfInfo.type),
              std::to_string(m_stSelfInfo.processId));
    XCLOG("Deregistered: %s", m_stSelfInfo.RedisKey().c_str());
    return true;
}

// =============================================================================
// 发现对等进程
// =============================================================================
std::vector<ProcessInfo> CServiceDiscovery::DiscoverPeers(EProcessType targetType) {
    std::vector<ProcessInfo> result;
    if (!m_bRedisAvailable) return result;

    auto members = RedisSMembers("cluster:process:by_type:" + std::to_string((int32)targetType));
    for (auto& idStr : members) {
        std::string key = "cluster:process:" + std::to_string((int32)targetType) + ":" + idStr;
        ProcessInfo info = ReadProcessInfo(key);
        if (info.processId != 0) {
            result.push_back(info);
        }
    }
    return result;
}

std::vector<ProcessInfo> CServiceDiscovery::GetAllRunningPeers() {
    std::vector<ProcessInfo> result;
    if (!m_bRedisAvailable) return result;

    for (int32 t = EProcessType_Gate; t <= EProcessType_World; t += 100) {
        if (t != 100 && t != 101 && t != 201 && t != 202) continue;
        auto peers = DiscoverPeers((EProcessType)t);
        for (auto& p : peers) {
            if (IsProcessRunning(p.type, p.processId)) {
                result.push_back(p);
            }
        }
    }
    return result;
}

ProcessInfo CServiceDiscovery::GetProcessInfo(EProcessType type, int64 processId) {
    std::string key = "cluster:process:" + std::to_string((int32)type) + ":" + std::to_string(processId);
    return ReadProcessInfo(key);
}

bool CServiceDiscovery::IsProcessRunning(EProcessType type, int64 processId) const {
    std::string key = "cluster:process:" + std::to_string((int32)type) + ":" + std::to_string(processId);
    auto* pCli = const_cast<CServiceDiscovery*>(this)->GetRedisCli();
    if (!pCli) return false;
    // 检查心跳是否在 15 秒内
    RecordDataST data;
    std::string hbKey = key;
    if (pCli->HGet(key, "heartbeat", &data) == REDIS_OK) {
        int64 hb = strtoll(std::string(data.Data(), data.Size()).c_str(), nullptr, 10);
        int64 now = (int64)time(nullptr);
        return (now - hb) <= 15;
    }
    return false;
}

// =============================================================================
// 链接规则
// =============================================================================
std::map<EProcessType, std::vector<EProcessType>> CServiceDiscovery::GetLinkRules() {
    std::map<EProcessType, std::vector<EProcessType>> rules;
    if (!m_bRedisAvailable) {
        // 默认规则: 所有类型互联
        std::vector<EProcessType> all = {
            EProcessType_Gate, EProcessType_Login,
            EProcessType_Player, EProcessType_World
        };
        for (auto& t : all) rules[t] = all;
        return rules;
    }
    // 从 Redis Hash 读取
    for (int32 t = EProcessType_Gate; t <= EProcessType_World; t += 100) {
        if (t != 100 && t != 101 && t != 201 && t != 202) continue;
        std::string val = RedisHGet("cluster:link:rules", std::to_string(t));
        if (!val.empty()) {
            std::vector<EProcessType> targets;
            // 逗号分隔
            size_t pos = 0;
            while (pos < val.size()) {
                size_t comma = val.find(',', pos);
                std::string tok = val.substr(pos, comma - pos);
                int32 tt = atoi(tok.c_str());
                if (tt >= 100) targets.push_back((EProcessType)tt);
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
            rules[(EProcessType)t] = targets;
        }
    }
    return rules;
}

bool CServiceDiscovery::SetLinkRule(EProcessType fromType, const std::vector<EProcessType>& toTypes) {
    if (!m_bRedisAvailable) return false;
    std::string val;
    for (size_t i = 0; i < toTypes.size(); ++i) {
        if (i > 0) val += ",";
        val += std::to_string((int32)toTypes[i]);
    }
    return RedisHSet("cluster:link:rules", std::to_string((int32)fromType), val);
}

// =============================================================================
// ID 生成
// =============================================================================
int64 CServiceDiscovery::GenerateProcessId(EProcessType type) {
    if (!m_bRedisAvailable) return -1;
    return RedisIncr("cluster:process:id_seq:" + std::to_string((int32)type));
}

// =============================================================================
// Redis 辅助方法
// =============================================================================
CRedisCli* CServiceDiscovery::GetRedisCli() {
    return CRedisPoolMgr::getMe().GetRedisCli(m_strRedisPoolTitle);
}

bool CServiceDiscovery::WriteProcessInfo(const ProcessInfo& info) {
    auto* pCli = GetRedisCli();
    if (!pCli) return false;

    bool ok = true;
    auto hset = [&](const char* field, const std::string& val) {
        if (!ok) return;
        RecordDataST d;
        d.Resize((int32)val.size());
        memcpy(d.MutableData(), val.data(), val.size());
        d.SetSize((int32)val.size());
        if (pCli->HSet(info.RedisKey(), field, &d) != REDIS_OK) ok = false;
    };
    hset("type",   std::to_string((int32)info.type));
    hset("id",     std::to_string(info.processId));
    hset("name",   info.processName);
    hset("host",   info.host);
    hset("port",   std::to_string(info.grpcPort));
    hset("load",   std::to_string(info.loadScore));
    hset("state",  std::to_string((int32)info.state));
    hset("heartbeat", std::to_string(info.lastHeartbeat));
    return ok;
}

ProcessInfo CServiceDiscovery::ReadProcessInfo(const std::string& redisKey) {
    ProcessInfo info;
    auto* pCli = GetRedisCli();
    if (!pCli) return info;

    auto hget = [&](const char* field) -> std::string {
        RecordDataST d;
        if (pCli->HGet(redisKey, field, &d) == REDIS_OK && d.Size() > 0)
            return std::string(d.Data(), d.Size());
        return "";
    };
    info.type          = (EProcessType)atoi(hget("type").c_str());
    info.processId     = atoll(hget("id").c_str());
    info.processName   = hget("name");
    info.host          = hget("host");
    info.grpcPort      = (uint16)atoi(hget("port").c_str());
    info.loadScore     = atoi(hget("load").c_str());
    info.state         = (EProcessState)atoi(hget("state").c_str());
    info.lastHeartbeat = atoll(hget("heartbeat").c_str());
    return info;
}

bool CServiceDiscovery::RedisHSet(const std::string& key, const std::string& field, const std::string& value) {
    auto* pCli = GetRedisCli();
    if (!pCli) return false;
    RecordDataST d;
    d.Resize((int32)value.size());
    memcpy(d.MutableData(), value.data(), value.size());
    d.SetSize((int32)value.size());
    return pCli->HSet(key, field, &d) == REDIS_OK;
}

std::string CServiceDiscovery::RedisHGet(const std::string& key, const std::string& field) {
    auto* pCli = GetRedisCli();
    if (!pCli) return "";
    RecordDataST d;
    if (pCli->HGet(key, field, &d) == REDIS_OK && d.Size() > 0)
        return std::string(d.Data(), d.Size());
    return "";
}

bool CServiceDiscovery::RedisSAdd(const std::string& key, const std::string& member) {
    auto* pCli = GetRedisCli();
    if (!pCli) return false;
    RecordDataST d;
    d.Resize((int32)member.size());
    memcpy(d.MutableData(), member.data(), member.size());
    d.SetSize((int32)member.size());
    // 使用通用 SET 命令的变体 — hiredis 没有直接 SADD, 用 redisCommand
    // 简化: 通过 redisCommand 执行
    redisContext* ctx = pCli->GetRedisContext();
    if (!ctx) return false;
    redisReply* reply = (redisReply*)redisCommand(ctx, "SADD %s %s", key.c_str(), member.c_str());
    bool ok = (reply && reply->type == REDIS_REPLY_INTEGER);
    if (reply) freeReplyObject(reply);
    return ok;
}

bool CServiceDiscovery::RedisSRem(const std::string& key, const std::string& member) {
    auto* pCli = GetRedisCli();
    if (!pCli) return false;
    redisContext* ctx = pCli->GetRedisContext();
    if (!ctx) return false;
    redisReply* reply = (redisReply*)redisCommand(ctx, "SREM %s %s", key.c_str(), member.c_str());
    bool ok = (reply && reply->type == REDIS_REPLY_INTEGER);
    if (reply) freeReplyObject(reply);
    return ok;
}

std::vector<std::string> CServiceDiscovery::RedisSMembers(const std::string& key) {
    std::vector<std::string> result;
    auto* pCli = GetRedisCli();
    if (!pCli) return result;
    redisContext* ctx = pCli->GetRedisContext();
    if (!ctx) return result;
    redisReply* reply = (redisReply*)redisCommand(ctx, "SMEMBERS %s", key.c_str());
    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; ++i) {
            if (reply->element[i]->type == REDIS_REPLY_STRING)
                result.push_back(std::string(reply->element[i]->str, reply->element[i]->len));
        }
    }
    if (reply) freeReplyObject(reply);
    return result;
}

int64 CServiceDiscovery::RedisIncr(const std::string& key) {
    auto* pCli = GetRedisCli();
    if (!pCli) return -1;
    redisContext* ctx = pCli->GetRedisContext();
    if (!ctx) return -1;
    redisReply* reply = (redisReply*)redisCommand(ctx, "INCR %s", key.c_str());
    int64 val = (reply && reply->type == REDIS_REPLY_INTEGER) ? reply->integer : -1;
    if (reply) freeReplyObject(reply);
    return val;
}

bool CServiceDiscovery::RedisSet(const std::string& key, const std::string& value) {
    auto* pCli = GetRedisCli();
    if (!pCli) return false;
    RecordDataST d;
    d.Resize((int32)value.size());
    memcpy(d.MutableData(), value.data(), value.size());
    d.SetSize((int32)value.size());
    return pCli->Set(key, &d) == REDIS_OK;
}

std::string CServiceDiscovery::RedisGet(const std::string& key) {
    auto* pCli = GetRedisCli();
    if (!pCli) return "";
    RecordDataST d;
    if (pCli->Get(key, &d) == REDIS_OK && d.Size() > 0)
        return std::string(d.Data(), d.Size());
    return "";
}
