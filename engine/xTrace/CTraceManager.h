#pragma once

#include "xBase/xSingleton.h"
#include "xCluster/xProcessDefine.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

// =============================================================================
// CTraceManager — 链路追踪管理模块 (协议延迟采样 + 慢消息上报)
//
// 职责:
//   1. 读取 WebPlatform 写入的延迟阈值
//        trace:config (hash) 字段 latency_threshold_ms (毫秒)
//   2. 供 RPC / Actor 消息路径调用, 采样协议延迟
//   3. 按阈值过滤后「本地缓冲」, 再周期性异步上报到 Redis:
//        阈值 <0   → 禁用上报
//        阈值 ==0  → 记录所有消息 (含 0ms, 用于显示完整链路)
//        阈值 >0   → 仅记录 latency > 阈值 的慢消息
//        trace:link:{fromType}:{fromId}:{toType}:{toId}  (ZSET, score=latency_ms)
//      member = {"direction":"send|recv","latency_ms":..,"msg_id":..,"ts":..}
//
// 上报字段: 发送方向(direction) + 延迟时间(latency_ms) 为必选; 其余字段后续扩展。
//
// 性能设计 (区别于早期「每条消息同步写 Redis」的实现):
//   - ReportProtocolLatency 只做内存操作: 按 linkKey 维护一份「最大延迟列表」
//     (每 link 固定容量 top-N 最小堆), 无任何 Redis/日志/磁盘 IO, 调度线程零阻塞。
//   - 独立上报线程每 kFlushIntervalSec (默认 60s) 醒来一次, 取走缓冲并在锁外
//     异步写 Redis (ZADD + 裁剪) + 打 slow report 日志。调度/RPC 线程不等它。
//   - 内存有界: 每 link 最多保留 TRACE_LINK_MAX_SAMPLES 条最高延迟样本。
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
    // 初始化: 绑定 Redis 连接 + 记录本进程身份 + 首次读取阈值 + 启动上报线程
    bool Init(const std::string &redisPoolTitle, const ProcessInfo &self);

    // 停止上报线程 (优雅关闭时调用, join 后台线程)
    void Stop();

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

    // 上报一条协议延迟 (内部按阈值过滤, 仅落本地内存缓冲, 无 IO)
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

    // 单条采样样本 (内存态, 上报线程序列化为 JSON)
    struct Sample
    {
        int64 latencyMs;
        uint32 msgId;
        int64 ts;
        int32 direction;
    };

    // 最小堆比较: latencyMs 小者靠前 (top = 当前 top-N 里的最小值, 便于淘汰)
    struct SampleGreater
    {
        bool operator()(const Sample &a, const Sample &b) const
        {
            return a.latencyMs > b.latencyMs;
        }
    };

    using SampleQueue = std::priority_queue<Sample, std::vector<Sample>, SampleGreater>;
    using PendingMap = std::map<std::string, SampleQueue>;

    // 构造 linkKey: trace:link:{fromType}:{fromId}:{toType}:{toId}
    std::string BuildLinkKey(const TraceEndpoint &from, const TraceEndpoint &to) const;

    // 取走当前本地缓冲 (换入空缓冲), 调用方在锁外执行 Redis 写
    PendingMap TakePending();

    // 上报线程入口: 每 kFlushIntervalSec 秒把本地缓冲刷到 Redis (异步)
    void FlushLoop();

    // 执行一次冲刷 (取缓冲 → 排序 → ZADD + 裁剪 + slow report)
    void FlushOnce();

private:
    std::string m_strRedisPoolTitle = "cluster_discovery";
    ProcessInfo m_selfInfo;
    std::atomic<int64> m_threshold{50}; // 默认阈值 50ms

    // 共享 Redis 句柄: Init 时从池取一条并长期持有 (RedisCluster 线程安全)
    // 仅上报线程 + 阈值刷新线程触碰, mutex 串行化逻辑操作 (热路径不碰)。
    class CRedisCli *m_pCli = nullptr;
    mutable std::mutex m_redisMutex;

    // 本地最大延迟缓冲: linkKey -> top-N 样本堆 (热路径只碰这里, 无 IO)
    std::mutex m_pendingMutex;
    PendingMap m_pending;

    // 异步上报线程
    std::thread m_flushThread;
    std::mutex m_stopMutex;
    std::condition_variable m_stopCv;
    std::atomic<bool> m_stop{false};
};
