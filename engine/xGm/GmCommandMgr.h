#pragma once

#include "xBase/xSingleton.h"
#include "xBase/xDefine.h"
#include "tService/tThridServiceConfig.h"
#include "xCluster/xProcessDefine.h"

#include <hiredis/hiredis.h>

#include <atomic>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// 前置声明: 完整类型在 .cpp 经 RedisMgr.h 引入 (避免此处 REDIS_OK 宏与 hiredis 冲突)
class CRedisCli;

// =============================================================================
// CGmCommandMgr — GM 命令模块 (Redis pub/sub 控制面)
//
// 用途: 让运维/WebPlatform 控制台能向「服务器框架层」与「业务 Actor」发送命令:
//   1. 订阅频道 (每个进程的 subscriber 线程长期 SUBSCRIBE):
//        cluster:gm:all           → 全集群广播
//        cluster:gm:type:{type}   → 定向某进程类型
//        cluster:gm:pid:{pid}     → 定向某进程实例
//   2. 框架命令 (C++ 直接在 GM 线程执行):
//        help / info / processes / actors / spawn / shutdown / loglevel
//   3. Actor 命令: `actor <type> <id> <cmd...>`
//        → CActorMgr::Find 后 PostMessage(SYS_GM_CMD), 由 CLuaActor 在调度线程
//          串行调用 Lua on_gm_cmd, 应答经 LogReply 写回 Redis 日志列表。
//
// 应答方向统一写 Redis 列表 (非 pub/sub, 便于 WebPlatform 轮询/分页读取):
//        cluster:gm:replies      → RPUSH + LTRIM (封顶 1000 条)
//
// Redis 连接设计:
//   m_cli — 共享 RedisCluster (线程安全), 承担写命令 (RPUSH/LTRIM/HSET/SADD/
//           SREM/DEL) 与 PUBLISH, 按 slot 自动路由, 无需 hash tag。
//   m_sub — 仅供 subscriber 线程使用的 hiredis 连接 (SUBSCRIBE 阻塞)。
//           pub/sub 在集群内全局广播、无 slot 路由, 单 seed 订阅即可收到全集群发布。
// =============================================================================
class CGmCommandMgr : public xSingleton<CGmCommandMgr>
{
public:
    CGmCommandMgr() = default;
    ~CGmCommandMgr() = default;

    // 初始化: 绑定 Redis (host/port/authkey) 与自身进程身份, 启动订阅线程
    bool Init(const ThridRedisConfig &cfg, EProcessType selfType, int64 selfPid);

    // 关闭订阅线程 (进程退出时调用)
    void Shutdown();

    // 写一条 GM 应答到 cluster:gm:replies 日志列表 (任意线程, 加时间戳/进程前缀)
    void LogReply(const std::string &text);

    // 发布消息到任意频道 (扩展用, 任意线程)
    bool Publish(const std::string &channel, const std::string &text);

    // Actor 实例注册/注销 (K8s 风格管理, 任意线程):
    //   cluster:actor:{type}:{id}      → Hash {type,id,gid,name,pid,ptype,state,created}
    //   cluster:actor:by_type:{type}   → Set  {id...}
    bool RegisterActor(EActorType type, int64 actorId, const std::string &name, uint64 globalId);
    bool DeregisterActor(EActorType type, int64 actorId);

    // 心跳续期: 刷新 cluster:actor:{type}:{id} 与 cluster:actor:by_type:{type} 的 TTL。
    // 进程每 5s 心跳时对每个托管 Actor 调用; 进程宕机后 TTL 过期由 Redis 自动清理,
    // 无需进程主动扫描 (僵尸 Actor 防护, 与进程注册同策略)。
    bool RefreshActorRegistration(EActorType type, int64 actorId);

    // 共享 RedisCluster 句柄 (线程安全) — 供 Actor 状态落盘/恢复、版本上报等复用。
    CRedisCli *GetCli() const
    {
        return m_cli;
    }

private:
    void SubscriberLoop();
    void HandleCommand(const std::string &message);
    std::string ExecFramework(const std::vector<std::string> &argv);
    void ForwardToActor(const std::vector<std::string> &argv);

    ThridRedisConfig m_cfg;
    EProcessType m_selfType = EProcessType_None;
    int64 m_selfPid = 0;

    CRedisCli *m_cli = nullptr;    // 共享 RedisCluster (线程安全, 写命令/发布/注册)
    redisContext *m_sub = nullptr; // 仅 subscriber 线程使用 (SUBSCRIBE 无 slot 路由)

    // 脚本热更 hash 门控: relpath → 已成功应用的 FNV-1a 内容 hash (仅 subscriber 线程读写)。
    // 收到 hotfix 命令时, 若拉取到的内容 hash 与上次应用相同则跳过 (避免无变更重复热更)。
    std::unordered_map<std::string, std::string> m_hotfixAppliedHash;

    std::thread m_thread;
    std::atomic<bool> m_running{false};
};
