
#include "xScheduler.h"



#include "xService/xService.h"



#include "xSchedulerMgr.h"






EventConroutineDefer::~EventConroutineDefer() {
    if (m_pScheduler->m_enSchedulerState == SchedulerStateType_Blocked_End) {
        // 释放消息
        delete m_pScheduler->m_ptrCurEvent;
        m_pScheduler->m_ptrCurEvent = nullptr;
        // 重置调度状态
        m_pScheduler->ResetScheduler();
        //
        if (m_pScheduler->GetSchedulerState() == SchedulerStateType_Ready) {
            xSchedulerMgr::getMe().AddScheduler(m_pScheduler);
        }
    }
}










