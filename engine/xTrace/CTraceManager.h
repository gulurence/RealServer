#pragma once

#include "xBase/xSingleton.h"
#include "xCluster/xProcessDefine.h"

#include <atomic>
#include <mutex>
#include <string>

// =============================================================================
// CTraceManager — 链路追踪管理模块 (协议延迟采样 + 慢消息上报)
//
// 职责:
//   1. 读取 WebPlatform 写入的延迟阈值
//        trace:config (hash) 字段 latency_threshold_ms (毫秒)
//   2. 供 RPC / Actor 消息路径调用, 采样协议延迟
//   3. 按阈值过滤后上报到 Redis:
//        阈值 <0   → 禁用上报
//        阈值 ==0  → 记录所有消息 (含 0ms, 用于显示完整链路)
//        阈值 >0   → 仅记录 latency > 阈值 的慢消息
//        trace:link:{fromType}:{fromId}:{toType}:{toId}  (ZSET, score=latency_ms)
//      member = {"direction":"send|recv","latency_ms":..,"msg_id":..,"ts":..}
//
// 上报字段: 发送方向(direction) + 延迟时间(latency_ms) 为必选; 其余字段后续扩展。
//
// 说明: 复用「共享句柄 + mutex 串行化」模式 (与 CServiceDiscovery 一致),
// 底层为线程安全的 RedisCluster, mutex 仅串行化逻辑操作。
// =============================================================================

// 发送方向
enum TraceDirection : int32
{
    TraceDir_Send = 0, // 本进程发出 (RPC 往返 / 消息投递)
    TraceDir_Recv = 1, // 本进程接收 (RPC 处理 / Actor 排队)
};

// 端点 (进程身份)
struct TraceEndpoint
{
    int32 type = 0;
    int64 id = 0;

    bool Valid() const
    {
        return type > 0 && id > 0;
    }
};

class CRedisCli;

class CTraceManager : public xSingleton<CTraceManager>
{
public:
    // 初始化: 绑定 Redis 连接 + 记录本进程身份 + 首次读取阈值
    bool Init(const std::string &redisPoolTitle, const ProcessInfo &self);

    // 设置本进程身份 (供后续上报区分 from/to)
    void SetSelfInfo(const ProcessInfo &self)
    {
        m_selfInfo = self;
    }

    // 本进程身份 (供 RPC 客户端给请求帧打上 fromProcessId)
    const ProcessInfo &GetSelfInfo() const
    {
        return m_selfInfo;
    }

    // 上报一条协议延迟 (内部按阈值过滤: <0 禁用 / ==0 全记录 / >0 仅慢消息)
    //   direction: TraceDir_Send/TraceDir_Recv
    //   latencyMs: 延迟 (毫秒)
    //   msgId:     协议号 (RPC methodId / Actor msgId)
    //   peer:      对端进程 (Send 方向为对端; Recv 方向可传空)
    void ReportProtocolLatency(TraceDirection direction, int64 latencyMs, uint32 msgId,
                               const TraceEndpoint &peer);

    // 当前阈值 (毫秒)
    int64 GetLatencyThreshold() const
    {
        return m_threshold.load(std::memory_order_relaxed);
    }

    // 从 Redis 重新读取阈值 (周期刷新)
    void RefreshThreshold();

private:
    int64 ReadThresholdFromRedis();
    class CRedisCli *GetRedisCli()
    {
        return m_pCli;
    }

private:
    std::string m_strRedisPoolTitle = "cluster_discovery";
    ProcessInfo m_selfInfo;
    std::atomic<int64> m_threshold{50}; // 默认阈值 50ms

    // 共享 Redis 句柄: Init 时从池取一条并长期持有 (RedisCluster 线程安全)
    class CRedisCli *m_pCli = nullptr;
    mutable std::mutex m_redisMutex;
};
