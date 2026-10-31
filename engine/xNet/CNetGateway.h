/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: CNetGateway.h
      Purpose:   Gate 网络接入层接口：网络层到 Actor 协程消息循环的桥接点。

      声明 CNetGateway 单例：把 xNetAgent 收到的网络包经 OnPackage 路由到 Gate Actor 的 PostMessage
      SendToCID 向指定连接发送 [uint32 len][body] 帧，KickConnection 供同账号并发登录踢旧使用
      数据流为 TCP→xNetAgent::Accept→PackageCheck 拆包→OnPackage→CActor::PostMessage 消息循环
  *****************************************************************/

#pragma once

#include "xNetDefine.h"
#include "xBase/xSingleton.h"
#include "CTokenBucketLimiter.h"

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>

class xNetAgent;
class CRedisCli;

// =============================================================================
// CNetGateway — 网络接入层 (新)
//
// 把 xNetAgent 收到的网络包 (NetPackageHeadST + body) 路由到目标 Actor 的
// 消息队列 (CActor::PostMessage)。这是"网络层 → Actor 协程消息循环"的桥接点。
//
// 数据流:
//   客户端 TCP 连接
//     → xNetAgent::Accept (asio 协程读入 xCircularBuffer)
//     → PackageCheck 线程拆出完整包
//     → CNetGateway::OnPackage (解析包头)
//     → CActorMgr::FindByActorID(targetActorId)
//     → CActor::PostMessage(msgId, body)  → Actor 协程 MessageLoop 消费
// =============================================================================
class CNetGateway : public xSingleton<CNetGateway>
{
public:
    CNetGateway();
    ~CNetGateway();

    // 启动监听端口，绑定网络包回调
    bool Start(uint16 u16Port);
    void Stop();

    // --- 网络回调 (由 xNetAgent 内部线程调用) ---
    NetCID OnAccept(void *pParam, tcp_socket &socket);
    void OnPackage(void *pParam, tcp_socket &socket, uint64 u64CID, NetPackageSharedPtr pPackage);
    void OnDisconnect(void *pParam, NetCID u64CID);

    // 连接建立/断开时注册 socket (供发送通道 SendToCID 使用; nullptr 表示断开)
    void OnSocketReady(void *pParam, NetCID u64CID, std::shared_ptr<tcp_socket> pSocket);

    // 发送通道: 向指定客户端连接发送一帧 [uint32 len][body]。
    // 线程安全 (可在任意线程调用, 内部投递到 socket 的 executor 异步写)。
    bool SendToCID(NetCID u64CID, const std::string &strBody);

    // 主动断开指定连接 (供 RPC_METHOD_KICK 同账号并发登录踢旧使用)。
    // 关闭 socket 后由 xNetAgent 触发 OnDisconnect → on_rpc_disconnect 完成收尾。
    void KickConnection(NetCID u64CID);

private:
    // 限流阈值 (5s TTL 缓存读 cluster:config:cluster 的 gate_rate_limit_per_sec)
    int32 GetRateLimitPerSec();

    // 主动断开连接 (限流超限)
    void Kick(NetCID u64CID);

private:
    NetAgentConfigST m_stConfig;
    std::unique_ptr<xNetAgent> m_pAgent;
    std::atomic<NetCID> m_nextCID{1};

    // CID → socket 映射 (发送通道)
    std::map<NetCID, std::shared_ptr<tcp_socket>> m_mapSockets;
    std::mutex m_sockMutex;

    // 本进程 Redis 连接 (读限流阈值; actor_cache 池, Start 时一次取得)
    CRedisCli *m_pRedis = nullptr;

    // 单连接限流
    CTokenBucketLimiter m_limiter;
    int32 m_rateLimitPerSec = 0;
    std::chrono::steady_clock::time_point m_lastRateRead{};
    std::mutex m_rateMutex;
};
