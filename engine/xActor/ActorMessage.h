#pragma once

#include "ActorDefine.h"
#include "xBase/xDefine.h"

#include <asio.hpp>

#include <chrono>
#include <coroutine>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <string>

// =============================================================================
// ActorMessage — 投递给 Actor 的一条消息
//   u32MsgId: 协议号 (后续与 proto msg id 对齐)
//   body:     二进制负载 (后续替换为 protobuf Message)
// =============================================================================
struct ActorMessage
{
    uint32 u32MsgId = 0;
    std::shared_ptr<std::string> body; // 为空表示停止信号

    // 入队时刻 (链路追踪: 用于计算排队延迟, 由 PostMessage 记录)
    std::chrono::steady_clock::time_point enqueuedAt{};
};

// =============================================================================
// ActorTask — Actor 协程任务类型 (最小实现)
// 仅作为协程返回类型，让 MessageLoop 能 co_await ActorMessageQueue::Pop()。
// fire-and-forget：挂起时句柄由队列的 waiter_ 持有，恢复由 Push 调度；
// final_suspend 为 suspend_never，co_return 时协程帧自动销毁。
// =============================================================================
struct ActorTask
{
    struct promise_type
    {
        ActorTask get_return_object()
        {
            return {};
        }
        std::suspend_never initial_suspend() noexcept
        {
            return {};
        }
        std::suspend_never final_suspend() noexcept
        {
            return {};
        }
        void return_void() noexcept
        {
        }
        void unhandled_exception() noexcept
        {
            std::terminate();
        }
    };
};

// =============================================================================
// ActorMessageQueue — 线程安全的 MPSC(多生产者单消费者) 消息队列
//
// 消费端：每个 Actor 的协程消息循环 (CActor::MessageLoop) 作为唯一消费者。
// 生产端：网络线程 / gRPC 线程 / 其他 Actor 协程，任意线程可 Push。
//
// 关键点：唤醒消费者时，通过 asio::post 把协程恢复动作调度到绑定的
// io_context 线程上，保证 Actor 业务逻辑始终在单一调度线程内执行
// (避免并发触碰进程唯一的 lua_State)。
//
// await_ready / await_suspend 在同一把 m_mtx 下重查队列，规避丢失唤醒竞态：
//   - await_suspend 返回 false = 期间已有消息到达，立即恢复；
//   - 否则登记 waiter_，Push 在锁内取出 waiter 并在锁外 resume。
// =============================================================================
class ActorMessageQueue
{
public:
    // 绑定协程恢复执行器 (调度器的 io_context)。不绑定则直接在 Push 线程恢复。
    void SetResumeContext(asio::io_context &ioc)
    {
        m_pIoc = &ioc;
    }

    void Push(ActorMessage msg)
    {
        std::coroutine_handle<> h = nullptr;
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            m_q.push_back(std::move(msg));
            if (m_waiter)
            {
                h = m_waiter;
                m_waiter = nullptr;
            }
        }
        if (h)
        {
            if (m_pIoc)
            {
                asio::post(*m_pIoc, [h] { h.resume(); });
            }
            else
            {
                h.resume();
            }
        }
    }

    // 协程可等待的出队操作
    struct PopAwaitable
    {
        ActorMessageQueue *self;

        bool await_ready()
        {
            std::lock_guard<std::mutex> lk(self->m_mtx);
            return !self->m_q.empty();
        }

        bool await_suspend(std::coroutine_handle<> h)
        {
            std::lock_guard<std::mutex> lk(self->m_mtx);
            if (!self->m_q.empty())
            {
                return false; // 期间已有消息到达，立即恢复
            }
            self->m_waiter = h;
            return true;
        }

        ActorMessage await_resume()
        {
            std::lock_guard<std::mutex> lk(self->m_mtx);
            ActorMessage m = std::move(self->m_q.front());
            self->m_q.pop_front();
            return m;
        }
    };

    PopAwaitable Pop()
    {
        return PopAwaitable{this};
    }

private:
    std::mutex m_mtx;
    std::deque<ActorMessage> m_q;
    std::coroutine_handle<> m_waiter = nullptr;
    asio::io_context *m_pIoc = nullptr;
};
