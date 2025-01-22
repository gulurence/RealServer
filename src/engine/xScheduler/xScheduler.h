#pragma once

#include "xBase/xCircularPool.h"
#include "xEvent/xEvent.h"
#include "xSchedulerDefine.h"


class xService;
// 协程任务的类
struct SchedulerTask
{
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    SchedulerTask(handle_type h) : handle(h) {}
    ~SchedulerTask() {
        //handle.destroy(); 
    }

    bool await_ready() {
        return false;  // 不立即完成
    }
    void await_resume() {
        std::cout << "RPC request completed. 3333333333 " << std::endl;
    }
    void await_suspend(std::coroutine_handle<> h) {
        //h.resume();  // RPC完成，恢复协程
    }
    handle_type handle;
};

// 协程任务的承诺类型（用于管理协程生命周期）
struct SchedulerTask::promise_type
{
    SchedulerTask get_return_object() {
        return SchedulerTask{ handle_type::from_promise(*this) };
    }

    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }


    void return_void() {}
    void unhandled_exception() { std::exit(1); }

public:
    xService* pService;
    PBEventPtr ptrEvent;
};


class ServiceScheduler;
class EventScheduler;
typedef std::shared_ptr<EventScheduler> EventSchedulerPtr;
typedef std::list<EventScheduler*> MsgSchedulerList;
// service handler
typedef SchedulerTask(*OnServiceProtoMsgCoroutineCallBack)(EventScheduler* msgPtr);
typedef void(*OnServiceProtoMsgCallBack)(EventScheduler* msgPtr);

struct EventScheduler
{
    friend ServiceScheduler;
public:
    EventScheduler(const SchedulerType &enSchedulerType, xService* pService, ServiceScheduler* pScheduler, PBEventPtr ptrEvent, OnServiceProtoMsgCallBack pCallSync, OnServiceProtoMsgCoroutineCallBack pCallCoroutin)
        :m_pService(pService), m_ptrEvent(ptrEvent), m_enSchedulerType(enSchedulerType), m_pCallSync(pCallSync), m_pCallCoroutine(pCallCoroutin),  m_pScheduler(pScheduler) {
    }

public:
    SchedulerType GetSchedulerType() {return m_enSchedulerType;}
    ServiceScheduler* GetServiceScheduler() { return m_pScheduler; }

public:
    xService* m_pService = nullptr;
    PBEventPtr m_ptrEvent = nullptr;

private:
    std::atomic<SchedulerType> m_enSchedulerType;

private:
    OnServiceProtoMsgCallBack m_pCallSync;
    OnServiceProtoMsgCoroutineCallBack m_pCallCoroutine;

private:
    ServiceScheduler* m_pScheduler = nullptr;
};


class EventConroutineDefer;
class ServiceScheduler
{
    friend EventConroutineDefer;
public:
    ServiceScheduler(int id) : id_(id) {
        m_enSchedulerState = SchedulerStateType_Wait;  // 不再使用 std::atomic
    }

    // 全局调用
    void PostRequest(const SchedulerType& enSchedulerType, xService* pService, PBEventPtr ptrEvent, OnServiceProtoMsgCallBack pCallSync, OnServiceProtoMsgCoroutineCallBack pCallCoroutin);
    void RunEvent();
    void ResetScheduler();

public:
    SchedulerType GetSchedulerType() {
        return m_enSchedulerType;
    }

    void SetSchedulerType(const SchedulerType& enType) {
        m_enSchedulerType = enType;
    }

    // 不再直接操作 atomic 状态，改为局部缓存
    SchedulerStateType GetSchedulerState() {
        return m_enSchedulerState;
    }

    void SetSchedulerState(const SchedulerStateType& enState) {
        // 在这里如果不希望做多线程同步操作，可以使用局部变量来进行控制
        m_enSchedulerState = enState;
    }

private:
    int id_;

private:
    SchedulerType m_enSchedulerType;  // 去掉 std::atomic
    SchedulerStateType m_enSchedulerState;  // 去掉 std::atomic

private:
    EventScheduler* m_ptrCurEvent=nullptr;
    xCircularPool<EventScheduler*> m_poolEvent;
};

typedef std::shared_ptr<ServiceScheduler> ServiceSchedulerPtr;
typedef std::list<ServiceSchedulerPtr> ServiceSchedulerList;

/*

CoroutineScheduler scheduler;
// 创建两个玩家
Player player1(1);
Player player2(2);
Player player3(3);
void msgLogic() {
    while (true) {
        // 启动调度器，恢复协程执行
        scheduler.run();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void sendMsg() {
    int index = 1;
    char message[128] = {0};
    while (true) {
        if (index < 10) {
            memset(message, 0, 128);
            sprintf_s(message, 100, "Message %d", index); index++;
            player1.postRequest([&]() { return messagecall1(&player1, message); });
            memset(message, 0, 128);
            sprintf_s(message, 100, "Message %d", index); index++;
            player1.postRequest([&]() { return messagecall2(&player1, message); });

            memset(message, 0, 128);
            sprintf_s(message, 100, "Message %d", 100000 + index); index++;
            player2.postRequest([&]() { return messagecall1(&player2, message); });
            memset(message, 0, 128);
            sprintf_s(message, 100, "Message %d", 100000 + index); index++;
            player2.postRequest([&]() { return messagecall2(&player2, message); });

            memset(message, 0, 128);
            sprintf_s(message, 100, "Message %d", 200000 + index); index++;
            player3.postRequest([&]() { return messagecall1(&player3, message); });
            memset(message, 0, 128);
            sprintf_s(message, 100, "Message %d", 200000 + index); index++;
            player3.postRequest([&]() { return messagecall2(&player3, message); });
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1)); // 模拟RPC延迟
    }
}

// 主函数，模拟多个 Player 协程
int main() {

    std::thread t1(msgLogic);
    t1.detach();

    std::thread t2(sendMsg);
    t2.detach();

    // 创建Player的协程
    auto task1 = player1.processRequests(scheduler);
    auto task2 = player2.processRequests(scheduler);
    auto task3 = player3.processRequests(scheduler);

    // 将Player的协程提交给调度器
    scheduler.post(task1.handle);
    scheduler.post(task2.handle);
    scheduler.post(task3.handle);

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 模拟RPC延迟
    }
    return 0;
}


*/

class EventConroutineDefer
{
public:
    EventConroutineDefer(ServiceScheduler *pScheduler) :m_pScheduler(pScheduler){}
    ~EventConroutineDefer();

private:
    ServiceScheduler* m_pScheduler = nullptr;
};

