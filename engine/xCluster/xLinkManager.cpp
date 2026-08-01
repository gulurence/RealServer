#include "xLinkManager.h"
#include "xServiceDiscovery.h"
#include "xProcessManager.h"
#include "xRpc/xRpc.h"
#include "xErrorCode/xErrorCode.h"
#include "xLog/xLog.h"

// =============================================================================
// Init
// =============================================================================
bool CLinkManager::Init() {
    m_selfType = CProcessManager::getMe().GetSelfInfo().type;

    auto& disc = CServiceDiscovery::getMe();
    m_linkRules = disc.GetLinkRules();

    if (m_linkRules.empty()) {
        XWRN("[LinkManager] No link rules found — using default (all-to-all)");
        std::vector<EProcessType> all = {
            EProcessType_Gate, EProcessType_Login,
            EProcessType_Player, EProcessType_World
        };
        for (auto& t : all) {
            if (t != m_selfType) {
                m_linkRules[m_selfType].push_back(t);
            }
        }
    }

    XCLOG("Initialized: self=%s, link_rules=%zu entries",
         ProcessTypeName(m_selfType), m_linkRules.size());
    return true;
}

// =============================================================================
// DiscoverAndConnect
// =============================================================================
bool CLinkManager::DiscoverAndConnect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& disc = CServiceDiscovery::getMe();

    XCLOG("Discovering peers...");

    // 获取本进程类型需要连接的目标类型列表
    auto it = m_linkRules.find(m_selfType);
    if (it == m_linkRules.end()) {
        XCLOG("No link targets for self type %s", ProcessTypeName(m_selfType));
        return true;  // 本类型不需要连接任何其他类型
    }

    int32 connected = 0;
    int32 skipped  = 0;

    for (auto targetType : it->second) {
        auto peers = disc.DiscoverPeers(targetType);
        for (auto& peer : peers) {
            // 跳过自己
            if (peer.processId == CProcessManager::getMe().GetProcessId()
                && peer.type == m_selfType) {
                continue;
            }

            // 仅连接 Running 状态的节点
            if (!IsProcessRunning(peer.state)) {
                XCWRN("Skip non-running node: %s:%d @ %s:%u (state=%s)",
                     ProcessTypeName(peer.type), (int32)peer.processId,
                     peer.host.c_str(), peer.grpcPort,
                     ProcessStateName(peer.state));
                skipped++;
                continue;
            }

            // 检查是否已连接
            if (m_mapPeers.find(peer.processId) != m_mapPeers.end()) {
                continue;  // 已连接
            }

            // 建立连接
            if (ConnectToPeer(peer)) {
                connected++;
            }
        }
    }

    XCLOG("Discovery complete: connected=%d skipped=%d (non-running)",
         connected, skipped);
    return true;
}

// =============================================================================
// ConnectToPeer
// =============================================================================
bool CLinkManager::ConnectToPeer(const ProcessInfo& peer) {
    // 构建唯一的 channel 名称
    char channelName[128];
    snprintf(channelName, sizeof(channelName), "peer_%d_%lld",
             (int32)peer.type, (int64)peer.processId);

    // 通过 CRpcService 创建 gRPC channel 池
    auto& rpc = CRpcService::getMe();
    bool ok = rpc.ConnectToServer(channelName, 4, peer.host, peer.grpcPort);
    if (!ok) {
        XERR("[LinkManager] Failed to connect to %s:%lld @ %s:%u",
             ProcessTypeName(peer.type), (int64)peer.processId,
             peer.host.c_str(), peer.grpcPort);
        return false;
    }

    PeerConnection conn;
    conn.info        = peer;
    conn.channelName = channelName;
    conn.connectedAt = (int64)time(nullptr);
    conn.available   = true;

    m_mapPeers[peer.processId] = conn;

    XCLOG("Connected: %s:%lld @ %s:%u (channel=%s)",
         ProcessTypeName(peer.type), (int64)peer.processId,
         peer.host.c_str(), peer.grpcPort, channelName);
    return true;
}

// =============================================================================
// DisconnectFromPeer
// =============================================================================
bool CLinkManager::DisconnectFromPeer(int64 processId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_mapPeers.find(processId);
    if (it == m_mapPeers.end()) return false;

    it->second.available = false;
    XCLOG("Disconnected: peer %lld (%s:%lld)",
         processId, ProcessTypeName(it->second.info.type),
         (int64)it->second.info.processId);

    m_mapPeers.erase(it);
    return true;
}

// =============================================================================
// IsPeerAvailable
// =============================================================================
bool CLinkManager::IsPeerAvailable(int64 processId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_mapPeers.find(processId);
    if (it == m_mapPeers.end()) return false;
    return it->second.available && IsProcessRunning(it->second.info.state);
}

// =============================================================================
// GetBestPeer
// =============================================================================
ProcessInfo CLinkManager::GetBestPeer(EProcessType targetType) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    ProcessInfo best;
    int32 bestLoad = INT32_MAX;

    for (auto& [id, conn] : m_mapPeers) {
        if (conn.info.type != targetType) continue;
        if (!conn.available) continue;
        if (!IsProcessRunning(conn.info.state)) continue;

        if (conn.info.loadScore < bestLoad) {
            bestLoad = conn.info.loadScore;
            best = conn.info;
        }
    }

    return best;
}

std::vector<ProcessInfo> CLinkManager::GetAvailablePeers(EProcessType targetType) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ProcessInfo> result;

    for (auto& [id, conn] : m_mapPeers) {
        if (conn.info.type == targetType && conn.available
            && IsProcessRunning(conn.info.state)) {
            result.push_back(conn.info);
        }
    }
    return result;
}

// =============================================================================
// RefreshLoop
// =============================================================================
void CLinkManager::RefreshLoop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& disc = CServiceDiscovery::getMe();

    // 检查已连接节点状态
    std::vector<int64> toRemove;
    for (auto& [id, conn] : m_mapPeers) {
        // 从 Redis 获取最新状态
        ProcessInfo latest = disc.GetProcessInfo(conn.info.type, id);
        if (latest.processId == 0) {
            // 节点已从 Redis 注销
            toRemove.push_back(id);
            XCWRN("Peer removed from discovery: %lld", (int64)id);
        } else if (!IsProcessRunning(latest.state) && conn.available) {
            // 节点变为非运行状态 — 停止分配 Actor
            conn.available = false;
            conn.info.state = latest.state;
            XCWRN("Peer marked unavailable: %s:%lld (state=%s)",
                 ProcessTypeName(conn.info.type), (int64)id,
                 ProcessStateName(latest.state));
        } else if (IsProcessRunning(latest.state) && !conn.available) {
            // 节点恢复运行
            conn.available = true;
            conn.info.state = latest.state;
            XCLOG("Peer restored: %s:%lld", ProcessTypeName(conn.info.type), (int64)id);
        }
    }

    for (auto id : toRemove) {
        m_mapPeers.erase(id);
    }
}

// =============================================================================
// GetConnectedPeers
// =============================================================================
std::vector<PeerConnection> CLinkManager::GetConnectedPeers() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<PeerConnection> result;
    for (auto& [id, conn] : m_mapPeers) {
        result.push_back(conn);
    }
    return result;
}

// =============================================================================
// ShouldConnectTo
// =============================================================================
bool CLinkManager::ShouldConnectTo(EProcessType selfType, EProcessType peerType) const {
    auto it = m_linkRules.find(selfType);
    if (it == m_linkRules.end()) return false;
    for (auto t : it->second) {
        if (t == peerType) return true;
    }
    return false;
}
