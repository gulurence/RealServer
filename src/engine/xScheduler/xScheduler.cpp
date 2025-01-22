
#include "xScheduler.h"
#include "xService/xService.h"
#include "xSchedulerMgr.h"


// 全局调用
void ServiceScheduler::PostRequest(const SchedulerType& enSchedulerType, xService* pService, PBEventPtr ptrEvent, OnServiceProtoMsgCallBack pCallSync, OnServiceProtoMsgCoroutineCallBack pCallCoroutin) {
    //XERR("PostRequest \n");

    m_poolEvent.Push(new EventScheduler(enSchedulerType, pService, this, ptrEvent, pCallSync, pCallCoroutin));

    // 本地状态缓存
    SchedulerStateType currentState = m_enSchedulerState;
    if (currentState == SchedulerStateType_Wait) {
        // 状态更新只在必要时才进行
        ResetScheduler();
        m_enSchedulerState = SchedulerStateType_Ready;
        xSchedulerMgr::getMe().AddScheduler(this);
    }
}

// 逻辑调度线程执行
void ServiceScheduler::RunEvent() {
    // 本地缓存状态
    SchedulerStateType currentState = m_enSchedulerState;
    if (currentState != SchedulerStateType_Ready) {
        return;
    }

    if (m_ptrCurEvent) {
        if (m_ptrCurEvent->m_enSchedulerType == SchedulerType_Coroutine) {
            // 协程执行
            m_enSchedulerState = SchedulerStateType_Blocked;
            std::cout << "processRequests. begin" << std::endl;
            (m_ptrCurEvent->m_pCallCoroutine)(m_ptrCurEvent);
        } else if (m_ptrCurEvent->m_enSchedulerType == SchedulerType_Synchronous) {
            // 同步执行
            m_enSchedulerState = SchedulerStateType_Running;
            (m_ptrCurEvent->m_pCallSync)(m_ptrCurEvent);
            m_enSchedulerState = SchedulerStateType_Wait;
            // 释放消息
            delete m_ptrCurEvent;
            m_ptrCurEvent = nullptr;
            // 重置调度状态
            ResetScheduler();
            //
            if (m_enSchedulerState == SchedulerStateType_Ready) {
                xSchedulerMgr::getMe().AddScheduler(this);
            }
        }
    }
}

void ServiceScheduler::ResetScheduler() {
    if (!m_ptrCurEvent) {
        EventScheduler* pEventScheduler = nullptr;
        m_poolEvent.Pop(pEventScheduler);
        if (pEventScheduler) {
            m_ptrCurEvent = pEventScheduler;
            // 重置调度类型
            SetSchedulerType(pEventScheduler->GetSchedulerType());
            m_enSchedulerState = SchedulerStateType_Ready;
        } else {
            m_enSchedulerState = SchedulerStateType_Wait;
        }
    }
}

EventConroutineDefer::~EventConroutineDefer() {
    if (m_pScheduler->m_enSchedulerState == SchedulerStateType_Blocked_End) {
        // 释放消息
        delete m_pScheduler->m_ptrCurEvent;
        m_pScheduler->m_ptrCurEvent = nullptr;
        m_pScheduler->m_enSchedulerState = SchedulerStateType_Wait;
        // 重置调度状态
        m_pScheduler->ResetScheduler();
        //
        if (m_pScheduler->GetSchedulerState() == SchedulerStateType_Ready) {
            xSchedulerMgr::getMe().AddScheduler(m_pScheduler);
        }
    }
}





