#include "CRpcTcpServer.h"
#include "xLog/xLog.h"
#include "xTrace/CTraceManager.h"

#include <chrono>

bool CRpcTcpServer::Start(uint16 u16Port)
{
    m_u16Port = u16Port;
    if (!thread_start())
    {
        XERR("[RpcTcpServer] failed to start thread");
        return false;
    }
    XLOG("[RpcTcpServer] listening on 0.0.0.0:%u", u16Port);
    return true;
}

void CRpcTcpServer::Register(uint32 u32MethodId, RpcHandler handler)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_mapHandlers[u32MethodId] = std::move(handler);
}

awaitable<void> CRpcTcpServer::Session(tcp_socket socket)
{
    for (;;)
    {
        RpcFrameHeadST head;
        auto [e1, n1] = co_await asio::async_read(socket, asio::buffer(&head, RpcFrameHeadSize),
                                                  asio::as_tuple(asio::use_awaitable));
        if (e1 || n1 != RpcFrameHeadSize)
        {
            break;
        }
        if (head.u32Len > MAX_RPC_BODY_SIZE)
        {
            break;
        }

        std::string body(head.u32Len, '\0');
        auto [e2, n2] = co_await asio::async_read(socket, asio::buffer(body.data(), head.u32Len),
                                                  asio::as_tuple(asio::use_awaitable));
        if (e2 || n2 != head.u32Len)
        {
            break;
        }

        // 查 handler (拷贝出, 避免持锁执行)
        RpcHandler handler;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_mapHandlers.find(head.u32MethodId);
            if (it != m_mapHandlers.end())
            {
                handler = it->second;
            }
        }

        RpcHandlerResult result;
        if (handler)
        {
            // 链路追踪: 上报 RPC 服务端处理延迟 (Recv 方向, 仅慢消息)
            auto t0 = std::chrono::steady_clock::now();
            result = handler(body);
            int64 latencyMs = (int64)std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::steady_clock::now() - t0)
                                  .count();
            TraceEndpoint peer;              // 对端进程 ID 来自请求帧 fromProcessId
            peer.id = head.u32FromProcessId; // type 由 CTraceManager 经 CLinkManager 解析
            CTraceManager::instance()->ReportProtocolLatency(TraceDir_Recv, latencyMs,
                                                             head.u32MethodId, peer);
        }
        else
        {
            result.errCode = 1; // unknown method
            XWRN("[RpcTcpServer] unknown method id=%u", head.u32MethodId);
        }

        // 组装响应帧: 第三字段回传 errCode, 第四字段回传本进程 ID
        RpcFrameHeadST rspHead;
        rspHead.u32Len = (uint32)result.rspBody.size();
        rspHead.u32RequestId = head.u32RequestId;
        rspHead.u32MethodId = (uint32)result.errCode;
        rspHead.u32FromProcessId = (uint32)CTraceManager::instance()->GetSelfInfo().processId;

        std::string out;
        out.reserve(RpcFrameHeadSize + rspHead.u32Len);
        out.append(reinterpret_cast<const char *>(&rspHead), RpcFrameHeadSize);
        out.append(result.rspBody);

        auto [e3, n3] = co_await asio::async_write(socket, asio::buffer(out),
                                                   asio::as_tuple(asio::use_awaitable));
        if (e3)
        {
            break;
        }
    }
}

awaitable<void> CRpcTcpServer::Listener(uint16 u16Port)
{
    auto executor = co_await this_coro::executor;
    tcp_acceptor acceptor(executor, {tcp::v4(), u16Port});
    for (;;)
    {
        if (auto [e, socket] = co_await acceptor.async_accept(); socket.is_open())
        {
            co_spawn(executor, Session(std::move(socket)), detached);
        }
    }
}

void CRpcTcpServer::thread_proc()
{
    try
    {
        asio::io_context ioc(1);
        co_spawn(ioc, Listener(m_u16Port), detached);
        ioc.run();
    }
    catch (std::exception &e)
    {
        XERR("[RpcTcpServer] thread_proc exception: %s", e.what());
    }
}
