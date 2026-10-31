/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: CPlayerActorIndex.cpp
      Purpose:   player:{playerId} 到 actorId 路由索引实现：本地缓存加 Redis 双写。

      实现 CPlayerActorIndex 单例：Register/Deregister 维护本地 map 与 Redis player:{id} 键并设 30s TTL
      Resolve 本地缓存优先、miss 回查 Redis 并回填，Invalidate 仅清本地缓存
      另含 IncrPlayerCount/DecrPlayerCount 维护 cluster:load:player_count 供登录排队限流，Redis 操作由 m_mutex 串行化
  *****************************************************************/

#include "CPlayerActorIndex.h"
#include "xRedis/RedisMgr.h"
#include "xRedis/RedisCli.h"
#include "xLog/xLog.h"

#include <cstdlib>

namespace
{
// player:{playerId} 路由索引 TTL (秒): 30s ≈ 6 次心跳, 进程心跳续期;
// 进程宕机后由 Redis 过期自动清理 (僵尸 Player 路由索引防护)。
constexpr int64 kPlayerIndexTtlSeconds = 30;
// PlayerActor 总数计数器键 (登录排队限流): cluster:load:player_count
constexpr const char *kPlayerCountKey = "cluster:load:player_count";
}

// =============================================================================
// Init — 从指定 Redis 池 checkout 专用连接
// =============================================================================
bool CPlayerActorIndex::Init(const std::string &redisPoolTitle)
{
    m_pCli = CRedisPoolMgr::getMe().GetRedisCli(redisPoolTitle);
    if (!m_pCli)
    {
        XWRN("[PlayerIndex] Redis pool '%s' not available — index degraded to local-only",
             redisPoolTitle.c_str());
        return false;
    }
    XLOG("[PlayerIndex] initialized with Redis pool '%s' (dedicated connection)",
         redisPoolTitle.c_str());
    return true;
}

// =============================================================================
// PlayerKey — Redis 键: player:{playerId}
// =============================================================================
std::string CPlayerActorIndex::PlayerKey(int64 playerId)
{
    return "player:" + std::to_string(playerId);
}

// =============================================================================
// Register — 创建 Player Actor 成功后注册索引
// =============================================================================
bool CPlayerActorIndex::Register(int64 playerId, uint64 actorId)
{
    if (playerId <= 0)
    {
        XWRN("[PlayerIndex] Register: invalid playerId=%lld", (int64)playerId);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_mapPlayerToActor[playerId] = actorId;
    }

    // 写 Redis (失败不致命: standalone 模式下仅本地缓存可用)
    if (m_pCli && !RedisSet(PlayerKey(playerId), std::to_string(actorId)))
    {
        XWRN("[PlayerIndex] Register: redis set failed player=%lld actor=%llu", (int64)playerId,
             (uint64)actorId);
    }

    // 注册即设 TTL: 进程宕机后由 Redis 过期清理, 心跳续期见 RefreshTtl
    if (m_pCli)
    {
        RedisExpire(PlayerKey(playerId), kPlayerIndexTtlSeconds);
    }

    XLOG("[PlayerIndex] Register: player=%lld → actor=%llu", (int64)playerId, (uint64)actorId);
    return true;
}

// =============================================================================
// Deregister — 卸载 Player Actor 时删除索引
// =============================================================================
bool CPlayerActorIndex::Deregister(int64 playerId)
{
    if (playerId <= 0)
    {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_mapPlayerToActor.erase(playerId);
    }

    if (m_pCli)
    {
        RedisDel(PlayerKey(playerId));
    }

    XLOG("[PlayerIndex] Deregister: player=%lld", (int64)playerId);
    return true;
}

// =============================================================================
// Resolve — 查询 playerId → actorId (缓存优先, miss 回查 Redis 并回填)
// =============================================================================
uint64 CPlayerActorIndex::Resolve(int64 playerId)
{
    if (playerId <= 0)
    {
        return 0;
    }

    // 1. 本地缓存命中
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_mapPlayerToActor.find(playerId);
        if (it != m_mapPlayerToActor.end())
        {
            return it->second;
        }
    }

    // 2. 回查 Redis 并回填
    if (!m_pCli)
    {
        return 0;
    }
    std::string val = RedisGet(PlayerKey(playerId));
    if (val.empty())
    {
        return 0;
    }

    uint64 actorId = (uint64)strtoull(val.c_str(), nullptr, 10);
    if (actorId == 0)
    {
        return 0;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_mapPlayerToActor[playerId] = actorId;
    }

    XLOG("[PlayerIndex] Resolve: player=%lld → actor=%llu (redis miss, backfilled)",
         (int64)playerId, (uint64)actorId);
    return actorId;
}

// =============================================================================
// Invalidate — 清本地缓存, 迫使下次 Resolve 回查 Redis
// =============================================================================
void CPlayerActorIndex::Invalidate(int64 playerId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_mapPlayerToActor.erase(playerId);
}

void CPlayerActorIndex::RefreshTtl(int64 playerId)
{
    if (playerId <= 0 || !m_pCli)
    {
        return;
    }
    RedisExpire(PlayerKey(playerId), kPlayerIndexTtlSeconds);
}

// =============================================================================
// Redis 辅助 (hiredis 原生命令, 单连接需串行化)
// =============================================================================
bool CPlayerActorIndex::RedisSet(const std::string &key, const std::string &value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto *pCli = GetRedisCli();
    if (!pCli)
    {
        return false;
    }
    RecordDataST d((int32)value.size(), value.data());
    return pCli->Set(key, &d) == REDIS_OK;
}

std::string CPlayerActorIndex::RedisGet(const std::string &key)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto *pCli = GetRedisCli();
    if (!pCli)
    {
        return "";
    }
    RecordDataST d;
    if (pCli->Get(key, &d) != REDIS_OK || d.Size() <= 0)
    {
        return "";
    }
    return std::string(d.Data(), d.Size());
}

bool CPlayerActorIndex::RedisDel(const std::string &key)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto *pCli = GetRedisCli();
    if (!pCli)
    {
        return false;
    }
    return pCli->Del(key.c_str()) == REDIS_OK;
}

bool CPlayerActorIndex::RedisExpire(const std::string &key, int64 ttlSec)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto *pCli = GetRedisCli();
    if (!pCli)
    {
        return false;
    }
    return pCli->Expire(key, ttlSec) == REDIS_OK;
}

// =============================================================================
// IncrPlayerCount / DecrPlayerCount — PlayerActor 总数计数器
//
// 登录排队限流的数据源: Login 进程读 cluster:load:player_count 判定是否超
// player_actor_cap 上限。PlayerActor 创建成功后 Incr, 卸载时 Decr (覆盖本地
// 创建与跨进程 RPC 创建/卸载两条路径, 见 CActorMgr::Install / UnInstall)。
// Redis 不可用时返回 -1 (调用方忽略, 排队退化为不限流)。
// =============================================================================
int64 CPlayerActorIndex::IncrPlayerCount()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto *pCli = GetRedisCli();
    if (!pCli)
    {
        return -1;
    }
    return pCli->Incr(kPlayerCountKey);
}

int64 CPlayerActorIndex::DecrPlayerCount()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto *pCli = GetRedisCli();
    if (!pCli)
    {
        return -1;
    }
    return pCli->Decr(kPlayerCountKey);
}
