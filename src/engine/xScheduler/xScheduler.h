#pragma once

#include "xBase/xCircularPool.h"

#include "xEvent/xEvent.h"


class xService;


#pragma once

#include "xBase/xCircularPool.h"

#include "xEvent/xEvent.h"


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
        return true;  // 不立即完成
    }
    void await_resume() {
        //std::cout << "RPC request completed." << std::endl;
    }
    void await_suspend(std::coroutine_handle<> h) {
        //std::cout << "RPC request started, will take " << std::chrono::milliseconds(100).count() << " milliseconds." << std::endl;
        //std::thread([h, this] {
        //    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 模拟RPC延迟
        //    h.resume();  // RPC完成，恢复协程
        //    }).detach();
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

// service handler
typedef SchedulerTask(*OnServiceProtoMsgCallBack)(xService* pService, PBEventPtr msgPtr);

// 协程调度器，管理所有协程的调度
class CoroutineScheduler
{
public:
    void post(std::coroutine_handle<> coroutine) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(coroutine);
    }

    void run() {
        while (!queue_.empty()) {
            auto handle = queue_.front();
            queue_.pop();
            handle.resume(); // 恢复协程
        }
    }

public:
    std::mutex mutex_;
    std::queue<std::coroutine_handle<>> queue_;
};

struct MsgScheduler
{
    MsgScheduler(xService* pService_, PBEventPtr ptrEvent_, OnServiceProtoMsgCallBack call_) 
        :pService(pService_), ptrEvent(ptrEvent_), call(call_) {
    }
    xService* pService; 
    PBEventPtr ptrEvent;
    OnServiceProtoMsgCallBack call;
};
typedef std::list<MsgScheduler*> MsgSchedulerList;

// Player 类，每个 Player 处理自己的协议请求
class ServiceScheduler
{
public:
    ServiceScheduler(int id) : id_(id) {}

    // 向 Player 添加协议任务
    void postRequest(xService* pService, PBEventPtr ptrEvent, OnServiceProtoMsgCallBack call) {
        requests_.Push(new MsgScheduler(pService, ptrEvent, call));
    }

    // 处理所有协议请求
    //SchedulerTask processRequests(ServiceScheduler *pScheduler) {
    //    ServiceScheduler* pServiceScheduler = pScheduler;
    //        MsgScheduler *request;
    //        if (pServiceScheduler->requests_.Pop(request)) {
    //            // 取出并处理当前协议请求
    //            co_await (request->call)(request->pService, request->ptrEvent);  // 协程会在这里等待
    //        } else {
    //            //std::cout << "Player " << id_ << " has no more protocol to process, waiting for new ones..." << std::endl;
    //            std::this_thread::sleep_for(std::chrono::microseconds(1)); // 模拟等待新协议
    //        }
    //}

    SchedulerTask processRequests(ServiceScheduler* pScheduler) {
        ServiceScheduler* pServiceScheduler = pScheduler;
        MsgScheduler* request;
        if (pServiceScheduler->requests_.Pop(request)) {
            // 取出并处理当前协议请求
            co_await (request->call)(request->pService, request->ptrEvent);  // 协程会在这里等待
        } else {
            //std::cout << "Player " << id_ << " has no more protocol to process, waiting for new ones..." << std::endl;
            std::this_thread::sleep_for(std::chrono::microseconds(1)); // 模拟等待新协议
        }
    }

public:
    int id_;
    xCircularPool<MsgScheduler*> requests_;
};

typedef std::shared_ptr<ServiceScheduler> ServiceSchedulerPtr;

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


