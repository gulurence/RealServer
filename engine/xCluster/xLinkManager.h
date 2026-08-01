#pragma once

#include "xProcessDefine.h"
#include "xBase/xSingleton.h"
#include <map>
#include <vector>
#include <mutex>

// =============================================================================
// CLinkManager — 进程间 gRPC 链接管理器
//
// 职责:
//   1. 从 Redis 加载链接规则 (哪些类型需要互连)
//   2. 依据服务发现发现对等节点
//   3. 仅链接到 Running 状态的节点
//   4. 已链接但变为非 Running 的节点停止 Actor 分配
//   5. 定期刷新: 发现新节点, 移除失活节点
// =============================================================================
class CLinkManager : public xSingleton<CLinkManager>
{
public:
    // 初始化 — 加载链接规则
    bool Init();

    // 发现并连接 — 依据链接规则发现对等进程并建立 gRPC 连接
    // 跳过非 Running 状态的节点
    bool DiscoverAndConnect();

    // 连接到指定对等进程
    bool ConnectToPeer(const ProcessInfo& peer);

    // 断开与指定进程的连接
    bool DisconnectFromPeer(int64 processId);

    // 检查对等进程是否可用于 Actor 分配
    bool IsPeerAvailable(int64 processId) const;

    // 获取指定类型的最佳可用对等进程 (负载均衡: 选择负载最低的)
    ProcessInfo GetBestPeer(EProcessType targetType) const;

    // 获取指定类型的所有可用对等进程
    std::vector<ProcessInfo> GetAvailablePeers(EProcessType targetType) const;

    // 定期刷新 — 检测新节点 / 移除失活节点
    void RefreshLoop();

    // 获取已连接的对等进程列表
    std::vector<PeerConnection> GetConnectedPeers() const;

private:
    // 判断是否应该连接 (依据链接规则)
    bool ShouldConnectTo(EProcessType selfType, EProcessType peerType) const;

private:
    // 链接规则: 本进程类型 → 需要连接的目标类型列表
    std::map<EProcessType, std::vector<EProcessType>> m_linkRules;

    // 已连接的对等进程: processId → PeerConnection
    std::map<int64, PeerConnection> m_mapPeers;

    // 本进程类型
    EProcessType m_selfType = EProcessType_None;

    mutable std::mutex m_mutex;
};
