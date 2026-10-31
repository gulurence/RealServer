/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: CRpcTcpClient.cpp
      Purpose:   自建 asio-TCP RPC 客户端实现

      Connect 建立到 peer 的 TCP 连接并 co_spawn Session 协程读响应帧
      Submit 按 requestId 登记 pending 回调并写请求帧, CallSyncRaw 同步阻塞等待含超时
      Session 反查 pending 回调并上报 RPC 往返延迟
  *****************************************************************/

#include "CRpcTcpClient.h"
#include "xLog/xLog.h"
#include "xTrace/CTraceManager.h"

CRpcTcpClient::CRpcTcpClient() : m_ioc(std::make_shared<asio::io_context>())
{
}

CRpcTcpClient::~CRpcTcpClient()
{
}

void CRpcTcpClient::Init()
{
    if (!thread_start())
    {
        XERR("[RpcTcpClient] failed to start thread");
        return;
    }
    XLOG("[RpcTcpClient] io_context thread started");
}

void CRpcTcpClient::Stop()
{
    if (m_ioc)
    {
        m_ioc->stop();
        XLOG("[RpcTcpClient] stop requested — io_context stopped");
    }
}

bool CRpcTcpClient::Connect(int64 peerId, const std::string &host, uint16 port)
{
    std::promise<bool> prom;
    auto fut = prom.get_future();

    std::string reachable = host;
    if (reachable.empty() || reachable == "0.0.0.0")
    {
        reachable = "127.0.0.1";
    }

    asio::post(*m_ioc,
               [this, peerId, reachable, port, &prom]()
               {
                   try
                   {
                       auto sock = std::make_shared<tcp::socket>(m_ioc->get_executor());
                       asio::ip::address addr = asio::ip::make_address(reachable);
                       asio::error_code ec;
                       sock->connect(tcp::endpoint(addr, port), ec);
                       if (ec)
                       {
                           XERR("[RpcTcpClient] connect to %s:%u failed: %s", reachable.c_str(),
                                port, ec.message().c_str());
                           prom.set_value(false);
                           return;
                       }

                       auto conn = std::make_shared<Connection>();
                       conn->peerId = peerId;
                       conn->socket = sock;
                       conn->connected = true;
                       {
                           std::lock_guard<std::mutex> lock(m_connMutex);
                           m_connections[peerId] = conn;
                       }

                       co_spawn(m_ioc->get_executor(), Session(sock, peerId), detached);
                       XLOG("[RpcTcpClient] connected peer=%lld @ %s:%u", (int64)peerId,
                            reachable.c_str(), port);
                       prom.set_value(true);
                   }
                   catch (std::exception &e)
                   {
                       XERR("[RpcTcpClient] connect exception: %s", e.what());
                       prom.set_value(false);
                   }
               });

    return fut.get();
}

void CRpcTcpClient::Disconnect(int64 peerId)
{
    std::shared_ptr<Connection> conn;
    {
        std::lock_guard<std::mutex> lock(m_connMutex);
        auto it = m_connections.find(peerId);
        if (it == m_connections.end())
        {
            return;
        }
        conn = it->second;
        m_connections.erase(it);
    }

    // 关闭 socket: 投递到 io_context 线程执行, 使 Session 协程的 async_read
    // 因 eof/aborted 退出并自行清理。跨线程 close 通过 post 保证线程安全。
    if (conn && conn->socket)
    {
        auto sock = conn->socket;
        asio::post(*m_ioc,
                   [sock]()
                   {
                       asio::error_code ec;
                       sock->close(ec);
                   });
    }
    XLOG("[RpcTcpClient] disconnect peer=%lld", (int64)peerId);
}

uint32 CRpcTcpClient::Submit(int64 peerId, uint32 methodId, const std::string &reqBody,
                             std::function<void(int32, const std::string &)> cb)
{
    uint32 requestId = m_nextRequestId.fetch_add(1);

    auto pending = std::make_shared<PendingCall>();
    pending->cb = std::move(cb);
    pending->startTime = std::chrono::steady_clock::now();
    pending->methodId = methodId;
    pending->peerId = peerId;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pending[requestId] = pending;
    }

    std::shared_ptr<Connection> conn;
    {
        std::lock_guard<std::mutex> lock(m_connMutex);
        auto it = m_connections.find(peerId);
        if (it == m_connections.end())
        {
            std::lock_guard<std::mutex> lock2(m_mutex);
            m_pending.erase(requestId);
            pending->cb(1, ""); // 无连接
            return 0;
        }
        conn = it->second;
    }

    RpcFrameHeadST head;
    head.u32Len = (uint32)reqBody.size();
    head.u32RequestId = requestId;
    head.u32MethodId = methodId;
    head.u32FromProcessId = (uint32)CTraceManager::instance()->GetSelfInfo().processId;

    std::string frame;
    frame.reserve(RpcFrameHeadSize + reqBody.size());
    frame.append(reinterpret_cast<const char *>(&head), RpcFrameHeadSize);
    frame.append(reqBody);

    auto sock = conn->socket;
    asio::post(*m_ioc,
               [sock, frame]()
               {
                   asio::error_code ec;
                   asio::write(*sock, asio::buffer(frame), ec);
                   if (ec)
                   {
                       XERR("[RpcTcpClient] write failed: %s", ec.message().c_str());
                   }
               });
    return requestId;
}

RawRpcResult CRpcTcpClient::CallSyncRaw(int64 peerId, uint32 methodId, const std::string &reqBody,
                                        uint32 timeoutMs)
{
    auto prom = std::make_shared<std::promise<RawRpcResult>>();
    auto fut = prom->get_future();
    uint32 requestId = Submit(peerId, methodId, reqBody,
                              [prom](int32 errCode, const std::string &rspBody)
                              {
                                  RawRpcResult r;
                                  r.code = errCode;
                                  r.ok = (errCode == 0);
                                  r.rspBody = rspBody;
                                  prom->set_value(std::move(r));
                              });

    if (fut.wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::ready)
    {
        return fut.get();
    }
    // 超时: 移除 pending, 避免迟到响应触发已失效 promise + 泄漏
    if (requestId != 0)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pending.erase(requestId);
    }
    RawRpcResult r;
    r.code = 2; // timeout
    r.ok = false;
    return r;
}

awaitable<void> CRpcTcpClient::Session(std::shared_ptr<tcp::socket> socket, int64 peerId)
{
    for (;;)
    {
        RpcFrameHeadST head;
        auto [e1, n1] = co_await asio::async_read(*socket, asio::buffer(&head, RpcFrameHeadSize),
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
        auto [e2, n2] = co_await asio::async_read(*socket, asio::buffer(body.data(), head.u32Len),
                                                  asio::as_tuple(asio::use_awaitable));
        if (e2 || n2 != head.u32Len)
        {
            break;
        }

        // 响应帧: 第三字段是 errCode
        int32 errCode = (int32)head.u32MethodId;

        std::shared_ptr<PendingCall> pending;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_pending.find(head.u32RequestId);
            if (it != m_pending.end())
            {
                pending = it->second;
                m_pending.erase(it);
            }
        }
        if (pending)
        {
            // 链路追踪: 上报 RPC 往返延迟 (Send 方向, 仅慢消息)
            int64 latencyMs = (int64)std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::steady_clock::now() - pending->startTime)
                                  .count();
            TraceEndpoint peer;
            peer.id = pending->peerId; // type 由 CTraceManager 经 CLinkManager 解析
            CTraceManager::instance()->ReportProtocolLatency(TraceDir_Send, latencyMs,
                                                             pending->methodId, peer);

            pending->cb(errCode, body);
        }
    }

    // 连接断开 — 清理
    {
        std::lock_guard<std::mutex> lock(m_connMutex);
        m_connections.erase(peerId);
    }
    XWRN("[RpcTcpClient] peer=%lld disconnected", (int64)peerId);
}

void CRpcTcpClient::thread_proc()
{
    try
    {
        auto guard = asio::make_work_guard(*m_ioc);
        m_ioc->run();
    }
    catch (std::exception &e)
    {
        XERR("[RpcTcpClient] thread_proc exception: %s", e.what());
    }
}
