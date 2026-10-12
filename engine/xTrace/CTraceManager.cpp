#include "CTraceManager.h"

#include "xRedis/RedisMgr.h"
#include "xRedis/RedisCli.h"
#include "xCluster/xLinkManager.h"
#include "xLog/xLog.h"

#include <cstdio>
#include <ctime>

// Redis 键约定 (与 WebPlatform trace.php / trace_topology.php 一致)
#define TRACE_CONFIG_KEY "trace:config"
#define TRACE_CONFIG_FIELD "latency_threshold_ms"
#define TRACE_LINK_PREFIX "trace:link:"

// 每个 link ZSET 保留的最多样本数 (按延迟降序保留 top N)
#define TRACE_LINK_MAX_SAMPLES 500

bool CTraceManager::Init(const std::string &redisPoolTitle, const ProcessInfo &self)
{
    m_strRedisPoolTitle = redisPoolTitle;
    m_selfInfo = self;

    // 从池中 checkout 一条专用连接并长期持有 (进程生命周期内复用)
    m_pCli = CRedisPoolMgr::getMe().GetRedisCli(m_strRedisPoolTitle);
    if (!m_pCli)
    {
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
    XLOG("[TraceManager] Initialized: threshold=%lldms self=%s:%lld", (int64)GetLatencyThreshold(),
         ProcessTypeName(m_selfInfo.type), (int64)m_selfInfo.processId);
    return true;
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

    std::lock_guard<std::mutex> lock(m_redisMutex);
    if (!m_pCli)
    {
        return;
    }

    // link key: trace:link:{ft}:{fi}:{tt}:{ti}
    char linkKey[128] = {0};
    snprintf(linkKey, sizeof(linkKey), "%s%d:%lld:%d:%lld", TRACE_LINK_PREFIX, (int32)from.type,
             (int64)from.id, (int32)to.type, (int64)to.id);

    // 采样 JSON: 必选字段 direction + latency_ms, 其余后续扩展
    char sample[256] = {0};
    snprintf(sample, sizeof(sample),
             "{\"direction\":\"%s\",\"latency_ms\":%lld,\"msg_id\":%u,\"ts\":%lld}",
             direction == TraceDir_Send ? "send" : "recv", (int64)latencyMs, msgId,
             (int64)time(nullptr));

    m_pCli->ZAdd(linkKey, (int64)latencyMs, sample);

    // 裁剪: 按延迟降序保留 top N
    m_pCli->ZRemRangeByRank(linkKey, 0, -(TRACE_LINK_MAX_SAMPLES + 1));

    // 仅当处于「只记录慢消息」模式 (threshold>0) 且本条确实超阈值时才打 slow report;
    // threshold==0 是「记录所有消息」的采样模式, 逐条打日志会刷屏 (几 ms 也误报为 slow)。
    if (threshold > 0)
    {
        XCLOG("[TraceManager] slow report dir=%s latency=%lldms msg=%u link=%s",
              direction == TraceDir_Send ? "send" : "recv", (int64)latencyMs, msgId, linkKey);
    }
}
