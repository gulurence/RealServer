#include "CTraceManager.h"

#include "xRedis/RedisMgr.h"
#include "xRedis/RedisCli.h"
#include "xCluster/xLinkManager.h"
#include "xLog/xLog.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>

// Redis 键约定 (与 WebPlatform trace.php / trace_topology.php 一致)
#define TRACE_CONFIG_KEY "trace:config"
#define TRACE_CONFIG_FIELD "latency_threshold_ms"
#define TRACE_LINK_PREFIX "trace:link:"

// 每个 link ZSET 保留的最多样本数 (按延迟降序保留 top N)
static const int64 kLinkMaxSamples = 500;

// 本地缓冲 → Redis 的异步冲刷间隔 (秒)
static const int64 kFlushIntervalSec = 60;

bool CTraceManager::Init(const std::string &redisPoolTitle, const ProcessInfo &self)
{
    m_strRedisPoolTitle = redisPoolTitle;
    m_selfInfo = self;

    // 从池中 checkout 一条专用连接并长期持有 (进程生命周期内复用)
    m_pCli = CRedisPoolMgr::getMe().GetRedisCli(m_strRedisPoolTitle);
    if (!m_pCli)
    {
        m_threshold.store(-1, std::memory_order_relaxed); // 无 Redis: 禁用采样 (避免无谓缓冲)
        XWRN("[TraceManager] Redis pool '%s' not available — tracing disabled",
             m_strRedisPoolTitle.c_str());
        return false;
    }

    int64 th = ReadThresholdFromRedis();
    if (th >= 0)
    {
        // 阈值语义: <0 禁用; ==0 记录所有消息; >0 仅记录 latency > 阈值 的慢消息
        m_threshold.store(th, std::memory_order_relaxed);
    }
    XLOG("[TraceManager] Initialized: threshold=%lldms self=%s:%lld flushInterval=%llds",
         (int64)GetLatencyThreshold(), ProcessTypeName(m_selfInfo.type), (int64)m_selfInfo.processId,
         (int64)kFlushIntervalSec);

    // 启动异步上报线程
    m_stop.store(false, std::memory_order_relaxed);
    m_flushThread = std::thread([this] { FlushLoop(); });

    return true;
}

void CTraceManager::Stop()
{
    m_stop.store(true, std::memory_order_relaxed);
    m_stopCv.notify_all();
    if (m_flushThread.joinable())
    {
        m_flushThread.join();
    }
}

int64 CTraceManager::ReadThresholdFromRedis()
{
    std::lock_guard<std::mutex> lock(m_redisMutex);
    if (!m_pCli)
    {
        return -1;
    }

    RecordDataST d;
    if (m_pCli->HGet(TRACE_CONFIG_KEY, TRACE_CONFIG_FIELD, &d) != REDIS_OK || d.Size() <= 0)
    {
        return -1;
    }
    return strtoll(std::string(d.Data(), d.Size()).c_str(), nullptr, 10);
}

void CTraceManager::RefreshThreshold()
{
    int64 th = ReadThresholdFromRedis();
    if (th >= 0)
    {
        m_threshold.store(th, std::memory_order_relaxed);
    }
}

std::string CTraceManager::BuildLinkKey(const TraceEndpoint &from, const TraceEndpoint &to) const
{
    char linkKey[128] = {0};
    snprintf(linkKey, sizeof(linkKey), "%s%d:%lld:%d:%lld", TRACE_LINK_PREFIX, (int32)from.type,
             (int64)from.id, (int32)to.type, (int64)to.id);
    return std::string(linkKey);
}

void CTraceManager::ReportProtocolLatency(TraceDirection direction, int64 latencyMs, uint32 msgId,
                                          const TraceEndpoint &peer)
{
    // 阈值过滤: <0 禁用; ==0 记录所有; >0 仅记录 latency > 阈值 的慢消息
    int64 threshold = GetLatencyThreshold();
    if (threshold < 0)
    {
        return;
    }
    if (threshold > 0 && latencyMs <= threshold)
    {
        return;
    }

    // 解析端点: Send 方向 from=self to=peer; Recv 方向 from=peer to=self
    TraceEndpoint from, to;
    if (direction == TraceDir_Send)
    {
        from.type = (int32)m_selfInfo.type;
        from.id = m_selfInfo.processId;
        to = peer;
    }
    else
    {
        from = peer;
        to.type = (int32)m_selfInfo.type;
        to.id = m_selfInfo.processId;
    }

    // 对端 (非 self) 端点的 type 未知时, 经 CLinkManager 解析
    //   Send: from=self to=peer  → 解析 to
    //   Recv: from=peer to=self  → 解析 from
    TraceEndpoint &peerEp = (direction == TraceDir_Send) ? to : from;
    if (peerEp.id > 0 && peerEp.type <= 0)
    {
        ProcessInfo info = CLinkManager::instance()->GetPeerInfo(peerEp.id);
        peerEp.type = (int32)info.type;
    }

    const std::string linkKey = BuildLinkKey(from, to);

    Sample s;
    s.latencyMs = latencyMs;
    s.msgId = msgId;
    s.ts = (int64)std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();
    s.direction = (int32)direction;

    // 本地缓冲: 每 link 维护 top-N 最大延迟列表 (最小堆, 超限淘汰最小者)
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    SampleQueue &q = m_pending[linkKey];
    if ((int64)q.size() < kLinkMaxSamples)
    {
        q.push(std::move(s));
    }
    else if (latencyMs > q.top().latencyMs)
    {
        q.pop();
        q.push(std::move(s));
    }
}

CTraceManager::PendingMap CTraceManager::TakePending()
{
    PendingMap taken;
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    taken.swap(m_pending);
    return taken;
}

void CTraceManager::FlushLoop()
{
    while (true)
    {
        {
            std::unique_lock<std::mutex> lock(m_stopMutex);
            bool stopped = m_stopCv.wait_for(lock, std::chrono::seconds(kFlushIntervalSec),
                                             [this] { return m_stop.load(std::memory_order_relaxed); });
            if (stopped)
            {
                break;
            }
        }
        FlushOnce();
    }

    // 退出前最后冲刷一次, 避免残留样本丢失
    FlushOnce();
}

void CTraceManager::FlushOnce()
{
    PendingMap pending = TakePending();
    if (pending.empty())
    {
        return;
    }

    int64 threshold = GetLatencyThreshold();

    std::lock_guard<std::mutex> lock(m_redisMutex);
    if (!m_pCli)
    {
        return;
    }

    for (auto &[linkKey, q] : pending)
    {
        // 取出堆内样本并降序排列 (最高延迟在前)
        std::vector<Sample> samples;
        samples.reserve(q.size());
        while (!q.empty())
        {
            samples.push_back(q.top());
            q.pop();
        }
        std::sort(samples.begin(), samples.end(),
                  [](const Sample &a, const Sample &b) { return a.latencyMs > b.latencyMs; });

        for (const Sample &s : samples)
        {
            char member[256] = {0};
            snprintf(member, sizeof(member),
                     "{\"direction\":\"%s\",\"latency_ms\":%lld,\"msg_id\":%u,\"ts\":%lld}",
                     s.direction == TraceDir_Send ? "send" : "recv", (int64)s.latencyMs, s.msgId,
                     (int64)s.ts);
            m_pCli->ZAdd(linkKey, s.latencyMs, member);
        }

        // 裁剪: 按延迟降序保留 top N
        m_pCli->ZRemRangeByRank(linkKey, 0, -(kLinkMaxSamples + 1));

        // slow report: 每 link 每 60s 只打本窗口最高延迟一条 (避免逐条刷屏)
        if (threshold > 0 && !samples.empty())
        {
            const Sample &top = samples.front();
            XCLOG("[TraceManager] slow report dir=%s latency=%lldms msg=%u link=%s",
                  top.direction == TraceDir_Send ? "send" : "recv", (int64)top.latencyMs, top.msgId,
                  linkKey.c_str());
        }
    }
}
