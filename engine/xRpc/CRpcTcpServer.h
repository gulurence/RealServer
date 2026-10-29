#pragma once

#include "RpcFrame.h"
#include "xBase/xSingleton.h"
#include "xBase/xThread.h"
#include "xNet/xNetDefine.h"

#include <asio.hpp>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>

// =============================================================================
// CRpcTcpServer — 自建 asio-TCP RPC 服务端 (替代 gRPC SRpcService)
//
// 单线程 io_context + acceptor, 每连接一个协程 (Session)。收到完整帧后按
// methodId 查 handler 注册表, 反序列化 → 调 handler → 回响应帧。
// =============================================================================
class CRpcTcpServer : public xSingleton<CRpcTcpServer>, public xThread
{
public:
    CRpcTcpServer()
    {
    }
    ~CRpcTcpServer()
    {
    }

    bool Start(uint16 u16Port);
    void Register(uint32 u32MethodId, RpcHandler handler);

    // 优雅关闭: 停 io_context 使 thread_proc 退出 (detach 线程随进程退出回收)。
    void Stop();

    virtual void thread_proc() override;

private:
    awaitable<void> Listener(uint16 u16Port);
    awaitable<void> Session(tcp_socket socket);

private:
    uint16 m_u16Port = 0;
    std::atomic<bool> m_bRunning = true;
    std::shared_ptr<asio::io_context> m_ioc;
    std::map<uint32, RpcHandler> m_mapHandlers;
    std::mutex m_mutex;
};
