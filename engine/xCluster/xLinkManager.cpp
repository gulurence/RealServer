#include "xLinkManager.h"
#include "xServiceDiscovery.h"
#include "xProcessManager.h"
#include "xRpc/CRpcTcpClient.h"
#include "xErrorCode/xErrorCode.h"
#include "xLog/xLog.h"

// =============================================================================
// Init
// =============================================================================
bool CLinkManager::Init()
{
    m_selfType = CProcessManager::getMe().GetSelfInfo().type;

    auto &disc = CServiceDiscovery::getMe();
    m_linkRules = disc.GetLinkRules();

    // 有向链接: 规则是「from_type → [to_type...]」的有向边, 每条边只建一条 TCP。
    // 无规则 = 本进程不连任何端 (不再回落全互联, 避免生成对称双向的双 TCP)。
    if (m_linkRules.empty())
    {
        XCLOG("Initialized: self=%s, no link rules (directed mode — no peers)",
              ProcessTypeName(m_selfType));
    }
    else
    {
        XCLOG("Initialized: self=%s, link_rules=%zu entries", ProcessTypeName(m_selfType),
              m_linkRules.size());
    }
    return true;
}

// =============================================================================
// DiscoverAndConnect
// =============================================================================
bool CLinkManager::DiscoverAndConnect()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto &disc = CServiceDiscovery::getMe();

    // 热生效: 每次发现前从 Redis 重读链接规则, 使 WebPlatform 写入 cluster:link:rules 后
    // 无需重启进程即生效 (默认每 10s 调用一次)。有向链接: 无规则 = 不连任何端。
    m_linkRules = disc.GetLinkRules();

    // 断开不再属于当前有向规则的对端 (有向热更新: 移除 gate→login 后即断开该连接)。
    // 无规则时 ShouldConnectTo 恒为 false → 断开全部。
    std::vector<int64> toDisconnect;
    for (auto &[id, conn] : m_mapPeers)
    {
        if (!ShouldConnectTo(m_selfType, conn.info.type))
        {
            toDisconnect.push_back(id);
        }
    }
    for (auto id : toDisconnect)
    {
        DisconnectFromPeerNoLock(id);
    }

    // 发现日志只在首次打印; 之后每 10s 的周期轮询处于稳态, 无需重复刷屏。
    const bool firstDiscovery = !m_discoveryLogged;
    if (firstDiscovery)
    {
        XCLOG("Discovering peers...");
    }

    // 获取本进程类型需要连接的目标类型列表
    auto it = m_linkRules.find(m_selfType);
    if (it == m_linkRules.end())
    {
        if (firstDiscovery)
        {
            XCLOG("No link targets for self type %s (directed mode)", ProcessTypeName(m_selfType));
        }
        m_discoveryLogged = true;
        return true; // 本类型不需要连接任何其他类型
    }

    int32 connected = 0;
    int32 skipped = 0;

    for (auto targetType : it->second)
    {
        auto peers = disc.DiscoverPeers(targetType);
        for (auto &peer : peers)
        {
            // 跳过自己
            if (peer.processId == CProcessManager::getMe().GetProcessId()
                && peer.type == m_selfType)
            {
                continue;
            }

            // 仅连接 Running 状态的节点
            if (!IsProcessRunning(peer.state))
            {
                XCWRN("Skip non-running node: %s:%d @ %s:%u (state=%s)", ProcessTypeName(peer.type),
                      (int32)peer.processId, peer.host.c_str(), peer.rpcPort,
                      ProcessStateName(peer.state));
                skipped++;
                continue;
            }

            // 检查是否已连接
            if (m_mapPeers.find(peer.processId) != m_mapPeers.end())
            {
                continue; // 已连接
            }

            // 建立连接
            if (ConnectToPeer(peer))
            {
                connected++;
            }
        }
    }

    if (firstDiscovery)
    {
        XCLOG("Discovery complete: connected=%d skipped=%d (non-running)", connected, skipped);
    }
    m_discoveryLogged = true;
    return true;
}

// =============================================================================
// ConnectToPeer
// =============================================================================
bool CLinkManager::ConnectToPeer(const ProcessInfo &peer)
{
    // 通过自建 asio-TCP RPC 客户端建立到 peer 的连接 (gRPC 退役)
    bool ok = CRpcTcpClient::instance()->Connect(peer.processId, peer.host, peer.rpcPort);
    if (!ok)
    {
        XERR("[LinkManager] Failed to connect to %s:%lld @ %s:%u", ProcessTypeName(peer.type),
             (int64)peer.processId, peer.host.c_str(), peer.rpcPort);
        return false;
    }

    PeerConnection conn;
    conn.info = peer;
    conn.channelName = "rpc:" + std::to_string(peer.processId); // 非 gRPC channel, 仅标识
    conn.connectedAt = (int64)time(nullptr);
    conn.available = true;

    m_mapPeers[peer.processId] = conn;

    XCLOG("Connected: %s:%lld @ %s:%u (rpc peer=%lld)", ProcessTypeName(peer.type),
          (int64)peer.processId, peer.host.c_str(), peer.rpcPort, (int64)peer.processId);
    return true;
}

// =============================================================================
// DisconnectFromPeer
// =============================================================================
bool CLinkManager::DisconnectFromPeer(int64 processId)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_mapPeers.find(processId) == m_mapPeers.end())
    {
        return false;
    }
    DisconnectFromPeerNoLock(processId);
    return true;
}

// =============================================================================
// DisconnectFromPeerNoLock — 调用方需已持有 m_mutex
// =============================================================================
void CLinkManager::DisconnectFromPeerNoLock(int64 processId)
{
    auto it = m_mapPeers.find(processId);
    if (it == m_mapPeers.end())
    {
        return;
    }

    it->second.available = false;
    XCLOG("Disconnected: peer %lld (%s:%lld)", processId, ProcessTypeName(it->second.info.type),
          (int64)it->second.info.processId);

    m_mapPeers.erase(it);

    // 真正关闭底层 TCP 连接, 而非仅移除逻辑记录 (有向热更新/主动断开时释放 socket)
    CRpcTcpClient::instance()->Disconnect(processId);
}

// =============================================================================
// IsPeerAvailable
// =============================================================================
bool CLinkManager::IsPeerAvailable(int64 processId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_mapPeers.find(processId);
    if (it == m_mapPeers.end())
    {
        return false;
    }
    return it->second.available && IsProcessRunning(it->second.info.state);
}

// =============================================================================
// GetBestPeer
// =============================================================================
ProcessInfo CLinkManager::GetBestPeer(EProcessType targetType) const
{
    // 候选先快照 (已连接且可用于分配者), 释放锁后再逐一到 Redis 拉最新负载,
    // 避免持锁期间做 Redis 往返阻塞链路刷新/连接等操作。
    std::vector<ProcessInfo> candidates;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto &[id, conn] : m_mapPeers)
        {
            if (conn.info.type == targetType && conn.available)
            {
                candidates.push_back(conn.info);
            }
        }
    }

    auto &disc = CServiceDiscovery::getMe();

    // 先收集通过全部硬过滤的候选 (Redis 可读、运行中、心跳新鲜、非排空、CPU/内存未满载、
    // Actor 数未超软上限)。过滤语义与旧逻辑一致, 只是把「挑最优」拆成两步以便平手轮询。
    std::vector<ProcessInfo> viable;
    viable.reserve(candidates.size());
    for (auto &cached : candidates)
    {
        // 被动拉取最新负载信息: 每次选节点都实时读 Redis, 取代本地缓存里陈旧的
        // loadScore/cpu/mem (缓存仅由 RefreshLoop 每 10s 刷新, 并发创建时会扎堆选同一低负载节点)。
        ProcessInfo info = disc.GetProcessInfo(cached.type, cached.processId);
        if (info.processId == 0)
        {
            continue; // Redis 读不到 (已注销/键过期): 不选陈旧节点
        }
        if (!IsProcessRunning(info.state))
        {
            continue;
        }
        // 心跳过滤: 跳过「state 仍为 Running 但心跳已停」的僵尸进程 (灰度强杀后 Hash 尚未过期)。
        // 与 CServiceDiscovery::DiscoverPeers 的心跳过滤一致, 避免选中已死对端导致 RPC 失败。
        if (disc.IsHeartbeatStale(info))
        {
            continue;
        }
        // 排空过滤: 排空中的进程不再接受新 Actor 分配 (Battle 进程级排空; 仍保持连接
        // 供既有 Actor 消息路由, 只是不选中承载新 Actor)。
        if (info.draining)
        {
            continue;
        }

        // 硬过滤: CPU/内存达到满载阈值则不再向该进程分配新 Actor (资源感知, P1 默认值)
        if (info.cpuPercent >= kCpuFullPercentDefault)
        {
            continue;
        }
        if (info.memPercent >= kMemFullPercentDefault)
        {
            continue;
        }
        // 软上限: 单进程 Actor 数超限则跳过 (0 = 不限)
        if (kActorMaxPerProcessDefault > 0 && info.loadScore >= kActorMaxPerProcessDefault)
        {
            continue;
        }

        viable.push_back(info);
    }

    if (viable.empty())
    {
        return ProcessInfo{};
    }

    // 加权挑选: Actor 数优先, 相同比 CPU, 再比内存 (与旧比较器语义一致)
    ProcessInfo best = viable[0];
    for (auto &info : viable)
    {
        if (info.loadScore < best.loadScore
            || (info.loadScore == best.loadScore && info.cpuPercent < best.cpuPercent)
            || (info.loadScore == best.loadScore && info.cpuPercent == best.cpuPercent
                && info.memPercent < best.memPercent))
        {
            best = info;
        }
    }

    // 平手轮询: 收集与最优 (loadScore/cpu/mem) 完全相同的候选, 轮询选一个。修复单例进程
    // (Login 等 actor 数恒等) 负载恒等时恒选 processId 最小者导致的扎堆单点 (login-01 吃满、
    // login-02/03 空转)。Game/Battle 负载不同时 ties.size()==1, 语义不变。
    std::vector<ProcessInfo> ties;
    ties.reserve(viable.size());
    for (auto &info : viable)
    {
        if (info.loadScore == best.loadScore && info.cpuPercent == best.cpuPercent
            && info.memPercent == best.memPercent)
        {
            ties.push_back(info);
        }
    }

    if (ties.size() <= 1)
    {
        return best;
    }
    size_t idx = (size_t)((m_rrCounter++) % (int64)ties.size());
    return ties[idx];
}

std::vector<ProcessInfo> CLinkManager::GetAvailablePeers(EProcessType targetType) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ProcessInfo> result;

    for (auto &[id, conn] : m_mapPeers)
    {
        if (conn.info.type == targetType && conn.available && IsProcessRunning(conn.info.state)
            && !CServiceDiscovery::getMe().IsHeartbeatStale(conn.info) && !conn.info.draining)
        {
            result.push_back(conn.info);
        }
    }
    return result;
}

ProcessInfo CLinkManager::GetPeerInfo(int64 processId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_mapPeers.find(processId);
    if (it == m_mapPeers.end())
    {
        return ProcessInfo();
    }
    return it->second.info;
}

// =============================================================================
// RefreshLoop
// =============================================================================
void CLinkManager::RefreshLoop()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto &disc = CServiceDiscovery::getMe();

    // 检查已连接节点状态
    std::vector<int64> toRemove;
    for (auto &[id, conn] : m_mapPeers)
    {
        // 从 Redis 获取最新状态
        ProcessInfo latest = disc.GetProcessInfo(conn.info.type, id);
        if (latest.processId == 0)
        {
            // 节点已从 Redis 注销
            toRemove.push_back(id);
            XCWRN("Peer removed from discovery: %lld", (int64)id);
        }
        else if (!IsProcessRunning(latest.state))
        {
            if (conn.available)
            {
                // 节点变为非运行状态 — 停止分配 Actor
                conn.available = false;
                conn.info.state = latest.state;
                XCWRN("Peer marked unavailable: %s:%lld (state=%s)",
                      ProcessTypeName(conn.info.type), (int64)id, ProcessStateName(latest.state));
            }
        }
        else if (disc.IsHeartbeatStale(latest))
        {
            // 心跳已停但 state 仍为 Running (灰度强杀后 Hash 尚未过期): 停止分配 Actor,
            // 与 DiscoverPeers 的心跳过滤一致。等 Hash TTL 到期 (30s) 后由 processId==0 分支移除。
            if (conn.available)
            {
                conn.available = false;
                conn.info = latest;
                XCWRN("Peer marked stale (heartbeat stopped): %s:%lld (state=%s)",
                      ProcessTypeName(conn.info.type), (int64)id, ProcessStateName(latest.state));
            }
        }
        else
        {
            // 节点运行中: 刷新最新负载/资源指标 (loadScore/cpu/mem), 供资源感知负载均衡使用
            bool wasAvailable = conn.available;
            conn.info = latest;
            if (!wasAvailable)
            {
                conn.available = true;
                XCLOG("Peer restored: %s:%lld", ProcessTypeName(conn.info.type), (int64)id);
            }
        }
    }

    for (auto id : toRemove)
    {
        m_mapPeers.erase(id);
    }
}

// =============================================================================
// GetConnectedPeers
// =============================================================================
std::vector<PeerConnection> CLinkManager::GetConnectedPeers() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<PeerConnection> result;
    for (auto &[id, conn] : m_mapPeers)
    {
        result.push_back(conn);
    }
    return result;
}

// =============================================================================
// ShouldConnectTo
// =============================================================================
bool CLinkManager::ShouldConnectTo(EProcessType selfType, EProcessType peerType) const
{
    auto it = m_linkRules.find(selfType);
    if (it == m_linkRules.end())
    {
        return false;
    }
    for (auto t : it->second)
    {
        if (t == peerType)
        {
            return true;
        }
    }
    return false;
}
