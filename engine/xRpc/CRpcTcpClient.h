#pragma once

#include "RpcFrame.h"
#include "xBase/xSingleton.h"
#include "xBase/xThread.h"
#include "xNet/xNetDefine.h"

#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <future>
#include <map>
#include <memory>
#include <mutex>

// =============================================================================
// CRpcTcpClient — 自建 asio-TCP RPC 客户端 (替代 gRPC CRpcService)
//
// 单线程 io_context + 线程池 (每 peer 一条连接), 提供:
//   - Call<Req,Rsp>(peerId, methodId, req, cb):  异步 lambda 回调 RPC
//   - CallSync<Req,Rsp>(peerId, methodId, req):  同步阻塞 RPC (供 Lua si_* 用)
//
// 请求关联: requestId 单调自增, 响应按 requestId 反查回调。
// =============================================================================
class CRpcTcpClient : public xSingleton<CRpcTcpClient>, public xThread
{
public:
    CRpcTcpClient();
    ~CRpcTcpClient();

    void Init(); // 启动 io_context 线程

    // 优雅关闭: 停 io_context 使 thread_proc 退出 (detach 线程随进程退出回收)。
    void Stop();

    bool Connect(int64 peerId, const std::string &host, uint16 port);

    // 主动断开与某对端的 TCP 连接 (有向链接规则热更新/主动移除时调用)
    void Disconnect(int64 peerId);

    // 异步 lambda 回调 RPC
    template <typename TReq, typename TRsp>
    uint32 Call(int64 peerId, uint32 methodId, const TReq &req,
                std::function<void(RpcCallResult<TRsp>)> cb)
    {
        std::string reqBody;
        req.SerializeToString(&reqBody);
        return Submit(peerId, methodId, reqBody,
                      [cb = std::move(cb)](int32 errCode, const std::string &rspBody)
                      {
                          RpcCallResult<TRsp> result;
                          result.code = errCode;
                          if (errCode == 0)
                          {
                              result.ok = result.reply.ParseFromString(rspBody);
                          }
                          cb(std::move(result));
                      });
    }

    // 同步阻塞 RPC (供 Lua si_* 阻塞接口使用; 超时返回 code=2)
    template <typename TReq, typename TRsp>
    RpcCallResult<TRsp> CallSync(int64 peerId, uint32 methodId, const TReq &req,
                                 uint32 timeoutMs = 5000)
    {
        auto prom = std::make_shared<std::promise<RpcCallResult<TRsp>>>();
        auto fut = prom->get_future();
        uint32 requestId
            = Call<TReq, TRsp>(peerId, methodId, req,
                               [prom](RpcCallResult<TRsp> r) { prom->set_value(std::move(r)); });

        if (fut.wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::ready)
        {
            return fut.get();
        }
        // 超时: 移除 pending, 避免迟到响应触发已失效 promise (future_error) 并防止泄漏
        if (requestId != 0)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_pending.erase(requestId);
        }
        RpcCallResult<TRsp> result;
        result.code = 2; // timeout
        result.ok = false;
        return result;
    }

    // 同步阻塞原始字节 RPC (reqBody/rspBody 为任意字节, 供非 protobuf 协议使用)
    RawRpcResult CallSyncRaw(int64 peerId, uint32 methodId, const std::string &reqBody,
                             uint32 timeoutMs = 5000);

    // 异步原始字节 RPC (fire-and-forget): 服务器主动推送无回包, 忽略业务响应。
    // 返回是否成功提交 (false = 无连接, Submit 返回 requestId==0); 调用方据此日志,
    // 避免推送被静默丢弃 (此前 void 返回导致 PushToGate 等无法感知无连接失败)。
    bool SubmitRaw(int64 peerId, uint32 methodId, const std::string &reqBody)
    {
        return Submit(peerId, methodId, reqBody, [](int32, const std::string &) {}) != 0;
    }

    virtual void thread_proc() override;

private:
    struct PendingCall
    {
        std::function<void(int32 errCode, const std::string &rspBody)> cb;
        std::chrono::steady_clock::time_point startTime; // 发送时刻 (链路追踪)
        uint32 methodId = 0;                             // RPC 方法号
        int64 peerId = 0;                                // 对端进程 ID
    };

    struct Connection
    {
        int64 peerId = 0;
        std::shared_ptr<tcp::socket> socket;
        bool connected = false;
    };

    // 内部: 提交一次调用 (类型擦除后的 body + 回调), 返回 requestId (0 = 无连接同步失败)
    uint32 Submit(int64 peerId, uint32 methodId, const std::string &reqBody,
                  std::function<void(int32, const std::string &)> cb);

    awaitable<void> Session(std::shared_ptr<tcp::socket> socket, int64 peerId);

private:
    std::shared_ptr<asio::io_context> m_ioc;
    std::atomic<uint32> m_nextRequestId{1};

    std::map<int64, std::shared_ptr<Connection>> m_connections;
    std::map<uint32, std::shared_ptr<PendingCall>> m_pending;
    std::mutex m_mutex;     // 保护 m_pending
    std::mutex m_connMutex; // 保护 m_connections
};
