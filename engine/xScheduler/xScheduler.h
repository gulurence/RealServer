#pragma once

#include "xBase/xCircularPool.h"
#include "xEvent/xEvent.h"
#include "xSchedulerDefine.h"


class xService;
// 处理异步消息协程封装
struct SchedulerTask
{
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    SchedulerTask(handle_type h) : handle(h) {}
    ~SchedulerTask() {
    }

    bool await_ready() {
        return false;
    }
    void await_resume() {
        //std::cout << "RPC request completed. 3333333333 " << std::endl;
    }
    void await_suspend(std::coroutine_handle<> h) {
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

// 用于event传递到调度器的封装
// event 执行完成之后会自动释放
struct EventScheduler
{
    friend ServiceScheduler;
public:
    EventScheduler(const SchedulerType &enSchedulerType, xService* pService, ServiceScheduler* pScheduler, PBEventPtr ptrEvent, OnServiceProtoMsgCallBack pCallSync, OnServiceProtoMsgCoroutineCallBack pCallCoroutin)
        :m_pService(pService), m_ptrEvent(ptrEvent), m_enSchedulerType(enSchedulerType), m_pCallSync(pCallSync), m_pCallCoroutine(pCallCoroutin),  m_pScheduler(pScheduler) {
    }
    ~EventScheduler() {
        m_ptrEvent = nullptr;
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

// 每个service对应的调度器 用于缓存EventScheduler
class ServiceScheduler
{
    friend EventConroutineDefer;
public:
    ServiceScheduler(int id) : id_(id) {
        m_enSchedulerState = SchedulerStateType_Wait;
    }

    // 全局调用
    void PostRequest(const SchedulerType& enSchedulerType, xService* pService, PBEventPtr ptrEvent, OnServiceProtoMsgCallBack pCallSync, OnServiceProtoMsgCoroutineCallBack pCallCoroutin);
    void RunEvent();
    void ResetScheduler();

public:
    SchedulerType GetSchedulerType() { return m_enSchedulerType; }
    void SetSchedulerType(const SchedulerType& enType) { m_enSchedulerType = enType;}
    SchedulerStateType GetSchedulerState() { return m_enSchedulerState; }
    void SetSchedulerState(const SchedulerStateType& enState) { m_enSchedulerState = enState; }

private:
    int id_;

private:
    std::atomic<SchedulerType> m_enSchedulerType;
    std::atomic <SchedulerStateType> m_enSchedulerState;

private:
    EventScheduler* m_ptrCurEvent = nullptr;
    xCircularPool<EventScheduler*> m_poolEvent;
};

typedef std::shared_ptr<ServiceScheduler> ServiceSchedulerPtr;
typedef std::list<ServiceSchedulerPtr> ServiceSchedulerList;


// 处理协程退出之前的下一个Event调度预处理
// 更新到下一个需要执行的EventScheduler
class EventConroutineDefer
{
public:
    EventConroutineDefer(ServiceScheduler *pScheduler) :m_pScheduler(pScheduler){}
    ~EventConroutineDefer();

private:
    ServiceScheduler* m_pScheduler = nullptr;
};

