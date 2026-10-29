#pragma once

#include "xProcessDefine.h"
#include "xBase/xSingleton.h"
#include <map>
#include <vector>
#include <mutex>

// =============================================================================
// CLinkManager — 进程间 asio-TCP RPC 链接管理器
//
// 职责:
//   1. 从 Redis 加载链接规则 (哪些类型需要互连)
//   2. 依据服务发现发现对等节点
//   3. 仅链接到 Running 状态的节点
//   4. 已链接但变为非 Running 的节点停止 Actor 分配
//   5. 定期刷新: 发现新节点, 移除失活节点
//
// 连接由 CRpcTcpClient::Connect 建立 (gRPC 已退役)。
// =============================================================================
class CLinkManager : public xSingleton<CLinkManager>
{
public:
    // 初始化 — 加载链接规则
    bool Init();

    // 发现并连接 — 依据链接规则发现对等进程并建立 asio-TCP RPC 连接
    // 跳过非 Running 状态的节点
    bool DiscoverAndConnect();

    // 连接到指定对等进程
    bool ConnectToPeer(const ProcessInfo &peer);

    // 断开与指定进程的连接
    bool DisconnectFromPeer(int64 processId);

    // 检查对等进程是否可用于 Actor 分配
    bool IsPeerAvailable(int64 processId) const;

    // 获取指定类型的最佳可用对等进程 (负载均衡: 选择负载最低的)。
    // 每次调用被动从 Redis 拉取各候选进程最新负载/资源/状态, 不用本地缓存里每 10s 才刷新的陈旧值。
    ProcessInfo GetBestPeer(EProcessType targetType) const;

    // 获取指定类型的所有可用对等进程
    std::vector<ProcessInfo> GetAvailablePeers(EProcessType targetType) const;

    // 获取指定 processId 的对等进程信息 (未连接则返回空 ProcessInfo)
    ProcessInfo GetPeerInfo(int64 processId) const;

    // 定期刷新 — 检测新节点 / 移除失活节点
    void RefreshLoop();

    // 获取已连接的对等进程列表
    std::vector<PeerConnection> GetConnectedPeers() const;

private:
    // 判断是否应该连接 (依据链接规则)
    bool ShouldConnectTo(EProcessType selfType, EProcessType peerType) const;

    // 断开并移除对端 (调用方需已持有 m_mutex), 同时关闭底层 TCP 连接
    void DisconnectFromPeerNoLock(int64 processId);

private:
    // 链接规则: 本进程类型 → 需要连接的目标类型列表
    std::map<EProcessType, std::vector<EProcessType>> m_linkRules;

    // 已连接的对等进程: processId → PeerConnection
    std::map<int64, PeerConnection> m_mapPeers;

    // 本进程类型
    EProcessType m_selfType = EProcessType_None;

    // 首次发现是否已打日志 (周期轮询不再重复打印 Discovering/Discovery complete)
    bool m_discoveryLogged = false;

    mutable std::mutex m_mutex;

    // 负载均衡平手轮询游标: 当多个候选进程 (loadScore/cpu/mem) 完全相同时, 轮询选取而非
    // 恒选 processId 最小者 (Login 等单例进程 actor 数恒等, 旧逻辑会扎堆单点)。
    mutable int64 m_rrCounter = 0;
};
