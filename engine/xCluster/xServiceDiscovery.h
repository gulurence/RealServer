#pragma once

#include "xProcessDefine.h"
#include "xBase/xSingleton.h"
#include <vector>
#include <map>
#include <string>
#include <functional>

// =============================================================================
// CServiceDiscovery — 基于 Redis 的服务发现与注册
//
// Redis 键模式:
//   cluster:process:{type}:{id}         → Hash {type,id,name,host,port,load,state,heartbeat}
//   cluster:process:by_type:{type}      → Set  {id1, id2, ...}
//   cluster:link:rules                   → Hash {from_type → "to_type1,to_type2,..."}
//   cluster:process:id_seq:{type}        → String  自增 ID 计数器
// =============================================================================
class CServiceDiscovery : public xSingleton<CServiceDiscovery>
{
public:
    // 初始化 — 绑定到指定的 Redis 连接池
    bool Init(const std::string& redisPoolTitle);

    // ---- 注册 / 心跳 / 注销 ----

    // 向 Redis 注册本进程信息
    bool Register(const ProcessInfo& info);

    // 心跳 — 更新状态 + 时间戳 (由定时器每 5 秒调用)
    bool Heartbeat();

    // 更新负载分数
    bool UpdateLoad(int32 loadScore);

    // 更新进程状态
    bool UpdateState(EProcessState newState);

    // 注销 (进程关闭时调用)
    bool Deregister();

    // ---- 服务发现 ----

    // 发现指定类型的所有进程
    std::vector<ProcessInfo> DiscoverPeers(EProcessType targetType);

    // 获取所有在运行的对等进程 (所有类型)
    std::vector<ProcessInfo> GetAllRunningPeers();

    // 获取指定进程的最新信息
    ProcessInfo GetProcessInfo(EProcessType type, int64 processId);

    // 检查进程是否在运行
    bool IsProcessRunning(EProcessType type, int64 processId) const;

    // ---- 链接规则 ----

    // 从 Redis 读取链接规则: fromType → [toType1, toType2, ...]
    std::map<EProcessType, std::vector<EProcessType>> GetLinkRules();

    // 设置链接规则 (运维初始化用)
    bool SetLinkRule(EProcessType fromType, const std::vector<EProcessType>& toTypes);

    // ---- ID 生成 ----

    // 为指定进程类型生成唯一自增 ID (Redis INCR)
    int64 GenerateProcessId(EProcessType type);

    // Redis 是否可用
    bool IsRedisAvailable() const { return m_bRedisAvailable; }

private:
    // 获取 Redis 客户端连接
    class CRedisCli* GetRedisCli();

    // 执行 Redis HSET 命令的辅助方法
    bool RedisHSet(const std::string& key, const std::string& field, const std::string& value);
    std::string RedisHGet(const std::string& key, const std::string& field);
    bool RedisSAdd(const std::string& key, const std::string& member);
    bool RedisSRem(const std::string& key, const std::string& member);
    std::vector<std::string> RedisSMembers(const std::string& key);
    int64 RedisIncr(const std::string& key);
    bool RedisSet(const std::string& key, const std::string& value);
    std::string RedisGet(const std::string& key);

    // 将 ProcessInfo 写入 Redis Hash
    bool WriteProcessInfo(const ProcessInfo& info);

    // 从 Redis Hash 读取 ProcessInfo
    ProcessInfo ReadProcessInfo(const std::string& redisKey);

private:
    std::string m_strRedisPoolTitle = "cluster_discovery";
    ProcessInfo m_stSelfInfo;
    bool        m_bRedisAvailable = false;
};
