#pragma once

#include "xBase/xSingleton.h"

#include <asio.hpp>
#include <atomic>
#include <future>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

class CActor;

// =============================================================================
// CActorScheduler — Actor 协程消息循环调度器
//
// 单 io_context + 工作线程，所有 Actor 的 MessageLoop 协程在此交错执行
// (协作式并发)。这与"进程唯一 lua_State" 的约束一致：业务逻辑永远在单一
// 线程内串行，无需加锁。
//
// 未来若需多核并行，可按 Actor 划分多个 io_context (每个一行程)，
// 并把每个 Actor 的 lua_State 隔离；当前阶段保持单线程最简单正确。
// =============================================================================
class CActorScheduler : public xSingleton<CActorScheduler>
{
public:
    CActorScheduler() = default;
    ~CActorScheduler() = default;

    // 启动工作线程 (u32ThreadCount 当前仅支持 1)
    bool Init(uint32 u32ThreadCount = 1);
    void Stop();

    // 将 Actor 的消息循环协程挂到调度器上
    void Spawn(CActor *pActor);

    // 延迟投递: delayMs 毫秒后向 actor(actorType, actorId) 投递一条消息。
    // 到点经 CActorMgr 重新 Find 定位实例 (避免跨延迟持有裸指针悬挂); 找不到则静默丢弃。
    void ScheduleAfter(int32 actorType, int64 actorId, uint32 delayMs, uint32 msgId,
                       const std::string &body);

    asio::io_context &GetContext()
    {
        return m_ioc;
    }

    // 同步投递到调度线程执行并阻塞等待结果。供 RPC 服务线程等"非调度线程"调用:
    // 进程唯一 lua_State 只能由调度线程触碰, 跨线程直接调 Lua (如 ActorRpcHandlers
    // 的 FORWARD/LOGIN 转发、Actor 创建/卸载) 会与消息循环并发损坏 Lua 栈, 表现为
    // 崩溃在 Lua 错误上报路径 (ldebug.c currentpc)。必须经此 post 到调度器串行执行。
    //
    // 注意: 不得在调度线程自身调用 (会自死锁); 调用方保证调度器已 Init 且在工作。
    template <typename Fn> auto DispatchSync(Fn &&fn) -> std::invoke_result_t<Fn>
    {
        using Ret = std::invoke_result_t<Fn>;
        auto promise = std::make_shared<std::promise<Ret>>();
        std::future<Ret> future = promise->get_future();
        asio::post(m_ioc, [fn = std::move(fn), promise]() mutable {
            try
            {
                if constexpr (std::is_void_v<Ret>)
                {
                    fn();
                    promise->set_value();
                }
                else
                {
                    promise->set_value(fn());
                }
            }
            catch (...)
            {
                promise->set_exception(std::current_exception());
            }
        });
        return future.get();
    }

private:
    asio::io_context m_ioc;
    // executor_work_guard 无默认构造、赋值被禁用，用 optional 延迟构造，
    // 以支持 Init/Stop 生命周期。
    std::optional<asio::executor_work_guard<asio::io_context::executor_type>> m_work;
    std::vector<std::thread> m_threads;
    std::atomic<bool> m_bRunning{false};
};
