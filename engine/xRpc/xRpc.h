#pragma once

#include "xBase/xDefine.h"
#include "xBase/xSingleton.h"
#include "xBase/xThread.h"
#include "xBase/xCircularPool.h"

#include "RpcFrame.h"
#include "CRpcTcpClient.h"

#include <asio.hpp>

#include <atomic>
#include <coroutine>
#include <functional>
#include <memory>
#include <string>
#include <utility>

// =============================================================================
// xRpc.h — 进程间 RPC (已从 gRPC 迁移到 asio + lambda)
//
// gRPC 版本 (SRpcService / CRpcService / gRPC 版 RpcCallAwaiter) 已全部退役:
//   - 服务端: CRpcTcpServer  (asio-TCP, methodId → lambda handler)
//   - 客户端: CRpcTcpClient  (asio-TCP, Call / CallSync / CallSyncRaw)
//   - 帧格式: RpcFrame.h 的 RpcFrameHeadST (超时 code=2, 无连接/未知方法 code=1)
//
// 本文件仅保留:
//   1. RpcCallMgr — 遗留通用工作线程池 (供 xNats/xService 遗留宏使用, 无 gRPC 依赖)
//   2. RpcCallAwaiter / AsyncRpcCall — asio+lambda 异步 RPC 等待原语 (C++20 协程),
//      恢复时必须走调度器 io_context, 永不阻塞调度线程
// =============================================================================

typedef std::function<void()> OnEventRpcCallBack;

// =============================================================================
// RpcCallMgr — 遗留通用工作线程 (阻塞调用投递线程)
//
// 单工作线程 + SPSC 队列: 用于把「阻塞式」调用 (遗留 Nats/xService 框架) 从
// 逻辑线程投递到独立线程执行。gRPC 退役后本类不再参与 RPC 主链路, 仅作为
// 遗留宏的兜底工作池保留, 待 xNats/xService 骨架清理时一并移除。
// =============================================================================
class RpcCallMgr : public xSingleton<RpcCallMgr>
{
public:
    void AddCall(OnEventRpcCallBack pCall);
    void Init();
    OnEventRpcCallBack PopCall();

public:
    std::atomic<bool> m_isRunning = true;

private:
    xCircularPool<OnEventRpcCallBack> m_poolEvent;
};

// =============================================================================
// RpcCallAwaiter — asio+lambda 异步 RPC 等待原语 (C++20 协程)
//
// 在 Actor 协程 (CActor::MessageLoop, 运行于 CActorScheduler 单线程 io_context) 中:
//
//     actormgr::PBRPCCreateActorRequest req; req.set_actor_type(...);
//     auto result = co_await AsyncRpcCall<actormgr::PBRPCCreateActorRequest,
//                                         actormgr::PBRPCCreateActorReply>(
//         peer.processId, RPC_METHOD_CREATE_ACTOR, req,
//         CActorScheduler::instance()->GetContext());
//     if (result.ok) { /* 使用 result.reply */ }
//
// 流程:
//   1. await_suspend 调 CRpcTcpClient::Call (非阻塞, 内部投递到客户端 io_context),
//      协程在此挂起;
//   2. 响应到达后经 asio::post 回到调度器 io_context, 在调度线程恢复协程;
//   3. await_resume 返回 RpcCallResult<TReply> (ok=false 表示传输失败/超时)。
//
// 约束: 协程挂起期间其帧(含本 awaiter)保持存活, 因此 await_suspend 捕获 this
// 是安全的; 前提是 Actor 不会在协程挂起等待 RPC 时被销毁 (当前阶段成立)。
// 与旧 gRPC 版不同: 本版不再依赖 RpcCallMgr 阻塞工作线程, 全程异步。
// =============================================================================
template <typename TRequest, typename TReply> struct RpcCallAwaiter
{
    RpcCallAwaiter(int64 peerId, uint32 methodId, const TRequest &request, asio::io_context &ioc)
        : m_peerId(peerId), m_methodId(methodId), m_request(request), m_ioc(ioc)
    {
    }

    bool await_ready() const noexcept
    {
        return false;
    }

    void await_suspend(std::coroutine_handle<> h)
    {
        // 非阻塞异步调用; 回调在客户端 io_context 线程执行, 完成后切回调度线程恢复协程
        CRpcTcpClient::instance()->Call<TRequest, TReply>(m_peerId, m_methodId, m_request,
                                                          [this, h](RpcCallResult<TReply> r)
                                                          {
                                                              m_result = std::move(r);
                                                              asio::post(m_ioc,
                                                                         [h] { h.resume(); });
                                                          });
    }

    RpcCallResult<TReply> await_resume()
    {
        return std::move(m_result);
    }

private:
    int64 m_peerId;
    uint32 m_methodId;
    TRequest m_request;
    asio::io_context &m_ioc;
    RpcCallResult<TReply> m_result;
};

// 便捷构造: 发起一次异步 RPC 并返回可等待对象
template <typename TRequest, typename TReply>
RpcCallAwaiter<TRequest, TReply> AsyncRpcCall(int64 peerId, uint32 methodId,
                                              const TRequest &request, asio::io_context &ioc)
{
    return RpcCallAwaiter<TRequest, TReply>(peerId, methodId, request, ioc);
}
