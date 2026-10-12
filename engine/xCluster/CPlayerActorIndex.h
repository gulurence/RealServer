#pragma once

#include "xBase/xSingleton.h"
#include <map>
#include <string>
#include <mutex>

// =============================================================================
// CPlayerActorIndex — player:{playerId} → actorId 路由索引
//
// 解决「按业务 PlayerID 定位其 Player Actor 所在进程」的问题:
//   1. 创建 Player Actor 时 Register(playerId, actorId) 写本地缓存 + Redis
//   2. 卸载 Player Actor 时 Deregister(playerId) 清本地缓存 + Redis
//   3. 交互时 Resolve(playerId): 先查本地缓存, miss 回查 Redis 并回填
//   4. Invalidate(playerId): 仅清本地缓存, 迫使下次 Resolve 回查 Redis
//      (目标疑似离线/迁移时调用)
//
// Redis 键约定 (与 cluster:process 同池, 复用 cluster_discovery 连接):
//   player:{playerId} → String (actorId 的十进制字符串)
//
// 本地缓存: map<playerId, actorId>。线程安全由 m_mutex 保证 (底层为线程安全的
// RedisCluster 共享句柄, m_mutex 串行化本地缓存与 Redis 访问)。
// =============================================================================
class CPlayerActorIndex : public xSingleton<CPlayerActorIndex>
{
public:
    CPlayerActorIndex()
    {
    }
    ~CPlayerActorIndex()
    {
    }

    // 初始化 — 从指定 Redis 池 checkout 一条专用连接长期持有
    // (默认复用 cluster_discovery 池, 与 CServiceDiscovery 同模式)
    bool Init(const std::string &redisPoolTitle = "cluster_discovery");

    // Redis 是否可用 (不可用时退化为「仅本地缓存」)
    bool IsRedisAvailable() const
    {
        return m_pCli != nullptr;
    }

    // 注册: playerId → actorId (创建 Player Actor 成功后调用)
    // 本地缓存必然更新; Redis 失败仅告警不致命 (standalone 模式仍可用)
    bool Register(int64 playerId, uint64 actorId);

    // 注销: 删除 playerId 的索引 (卸载 Player Actor 时调用)
    bool Deregister(int64 playerId);

    // 查询: 先本地缓存, miss 回查 Redis 并回填; 未找到返回 0
    uint64 Resolve(int64 playerId);

    // 失效: 仅清本地缓存条目 (目标疑似离线/迁移时调用, 迫使下次回查 Redis)
    void Invalidate(int64 playerId);

    // 心跳续期: 刷新 player:{playerId} 的 TTL (僵尸 Player 路由索引防护)。
    // 进程每 5s 心跳对每个 Player Actor 调用; 进程宕机后由 Redis 过期自动清理。
    void RefreshTtl(int64 playerId);

private:
    class CRedisCli *GetRedisCli() const
    {
        return m_pCli;
    }
    static std::string PlayerKey(int64 playerId);

    bool RedisSet(const std::string &key, const std::string &value);
    std::string RedisGet(const std::string &key);
    bool RedisDel(const std::string &key);
    bool RedisExpire(const std::string &key, int64 ttlSec);

private:
    // 共享 Redis 句柄: Init 时从池取一条并长期持有 (RedisCluster 线程安全)
    class CRedisCli *m_pCli = nullptr;

    // 保护本地缓存 + 串行化 Redis 上下文访问
    mutable std::mutex m_mutex;

    // 本地缓存: playerId → actorId
    std::map<int64, uint64> m_mapPlayerToActor;
};
