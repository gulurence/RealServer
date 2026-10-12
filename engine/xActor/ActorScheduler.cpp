#include "ActorScheduler.h"
#include "Actor.h"
#include "ActorMgr.h"
#include "xLog/xLog.h"

#include <chrono>
#include <memory>

bool CActorScheduler::Init(uint32 u32ThreadCount)
{
    if (m_bRunning)
    {
        return true;
    }
    if (u32ThreadCount < 1)
    {
        u32ThreadCount = 1;
    }

    // 保持 io_context 常活。协程挂起在非 asio 的 awaiter (ActorMessageQueue)
    // 上时不会为 io_context 增加工作项，若无 work guard，run() 会立刻返回。
    m_work.emplace(asio::make_work_guard(m_ioc));

    m_bRunning = true;
    for (uint32 i = 0; i < u32ThreadCount; ++i)
    {
        m_threads.emplace_back([this] { m_ioc.run(); });
    }
    XLOG("[ActorScheduler] started with %u thread(s)", u32ThreadCount);
    return true;
}

void CActorScheduler::Stop()
{
    if (!m_bRunning)
    {
        return;
    }
    m_bRunning = false;
    m_work.reset();
    m_ioc.stop();
    for (auto &t : m_threads)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
    m_threads.clear();
    XLOG("[ActorScheduler] stopped");
}

void CActorScheduler::Spawn(CActor *pActor)
{
    if (!pActor || pActor->IsMessageLoopSpawned())
    {
        return;
    }
    pActor->MarkMessageLoopSpawned();
    // 启动协程：initial_suspend = suspend_never，运行到首次 co_await 挂起后返回。
    // 挂起后句柄由 ActorMessageQueue 的 waiter_ 持有，Push 时经 asio::post 恢复。
    pActor->MessageLoop();
    XLOG("[ActorScheduler] spawned message loop for actor type=%d id=%lld", pActor->GetActorType(),
         pActor->GetActorID());
}

void CActorScheduler::ScheduleAfter(int32 actorType, int64 actorId, uint32 delayMs, uint32 msgId,
                                    const std::string &body)
{
    if (delayMs == 0)
    {
        delayMs = 1;
    }
    auto timer = std::make_shared<asio::steady_timer>(m_ioc);
    timer->expires_after(std::chrono::milliseconds(delayMs));
    // 到点经 CActorMgr 重新 Find 定位实例, 避免跨延迟持有裸指针悬挂 (Actor 已卸载则丢弃)。
    timer->async_wait(
        [timer, actorType, actorId, msgId, body](const asio::error_code &ec)
        {
            if (ec)
            {
                return; // 取消 / 调度器停止
            }
            CActor *pActor = CActorMgr::instance()->Find((EActorType)actorType, actorId);
            if (pActor)
            {
                pActor->PostMessage(msgId, body);
            }
        });
}
