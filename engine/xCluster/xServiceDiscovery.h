#pragma once

#include "xProcessDefine.h"
#include "xBase/xSingleton.h"
#include <vector>
#include <map>
#include <string>
#include <functional>
#include <mutex>

// =============================================================================
// CServiceDiscovery — 基于 Redis 的服务发现与注册
//
// Redis 键模式:
//   cluster:process:{type}:{id}         → Hash
//   {type,id,name,host,ip,port,load,state,heartbeat,authkey} cluster:process:by_type:{type}      →
//   Set  {id1, id2, ...} cluster:link:rules                   → Hash {from_type →
//   "to_type1,to_type2,..."} cluster:process:id_seq:{type}        → String  自增 ID 计数器
// =============================================================================
class CServiceDiscovery : public xSingleton<CServiceDiscovery>
{
public:
    // 初始化 — 绑定到指定的 Redis 连接池
    bool Init(const std::string &redisPoolTitle);

    // ---- 注册 / 心跳 / 注销 ----

    // 向 Redis 注册本进程信息
    bool Register(const ProcessInfo &info);

    // 确认节点详情 — 写入完整信息 (含 ip/authkey) 并读回比对, 一致才算节点初始化完成
    bool ConfirmNodeDetail(const ProcessInfo &info);

    // 心跳 — 更新状态 + 时间戳 (由定时器每 5 秒调用)
    bool Heartbeat();

    // 更新负载分数
    bool UpdateLoad(int32 loadScore);

    // 更新资源占用 (CPU% / RSS MB / 内存%) — 心跳时采样写入
    bool UpdateResource(int32 cpuPercent, int64 memRssMb, int32 memPercent);

    // 更新进程状态
    bool UpdateState(EProcessState newState);

    // 更新进程版本 + 游戏内容版本 (P9 灰度更新: 心跳时上报)
    bool UpdateVersion(const std::string &version, const std::string &contentVersion);

    // 读 cluster:update:content:config 字段 (游戏内容灰度配置)
    std::string GetContentUpdateConfig(const std::string &field);

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
    bool SetLinkRule(EProcessType fromType, const std::vector<EProcessType> &toTypes);

    // ---- ID 生成 ----

    // 为指定进程类型生成唯一自增 ID (Redis INCR)
    int64 GenerateProcessId(EProcessType type);

    // Redis 是否可用
    bool IsRedisAvailable() const
    {
        return m_bRedisAvailable;
    }

private:
    // 获取本模块专用 Redis 连接 (Init 时创建, 进程生命周期内复用)
    class CRedisCli *GetRedisCli();

    // 执行 Redis HSET 命令的辅助方法
    bool RedisHSet(const std::string &key, const std::string &field, const std::string &value);
    std::string RedisHGet(const std::string &key, const std::string &field);
    bool RedisSAdd(const std::string &key, const std::string &member);
    bool RedisSRem(const std::string &key, const std::string &member);
    std::vector<std::string> RedisSMembers(const std::string &key);
    int64 RedisIncr(const std::string &key);
    bool RedisSet(const std::string &key, const std::string &value);
    std::string RedisGet(const std::string &key);
    bool RedisExpire(const std::string &key, int64 ttlSec);

    // 将 ProcessInfo 写入 Redis Hash
    bool WriteProcessInfo(const ProcessInfo &info);

    // 从 Redis Hash 读取 ProcessInfo
    ProcessInfo ReadProcessInfo(const std::string &redisKey);

private:
    std::string m_strRedisPoolTitle = "cluster_discovery";
    ProcessInfo m_stSelfInfo;
    bool m_bRedisAvailable = false;

    // 共享 Redis 句柄: Init 时从池取一条并长期持有 (RedisCluster 线程安全,
    // 内部自带连接池与 slot 路由)。m_redisMutex 保留以串行化逻辑操作与日志。
    class CRedisCli *m_pCli = nullptr;
    mutable std::mutex m_redisMutex;
};
