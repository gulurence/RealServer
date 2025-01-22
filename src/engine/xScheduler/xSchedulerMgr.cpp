
#include "xSchedulerMgr.h"



xSchedulerMgr::xSchedulerMgr(uint32 u32CoroutineSchedulerCount, uint32 u32SysSchedulerCount)
    :m_u32CoroutineSchedulerCount(u32CoroutineSchedulerCount), m_u32SyncSchedulerCount(u32SysSchedulerCount) {

    m_pSysSchedulerProcess = new xSchedulerProcess*[SchedulerType_Max];

    for (uint32 i = 0; i < SchedulerType_Max; ++i) {
        if (i == SchedulerType_Coroutine) {
            m_pSysSchedulerProcess[i] = new xSchedulerProcess[m_u32CoroutineSchedulerCount];
        }
        if (i == SchedulerType_Synchronous) {
            m_pSysSchedulerProcess[i] = new xSchedulerProcess[m_u32SyncSchedulerCount];
        }
    }

    for (uint32 i = 0; i < SchedulerType_Max; ++i) {
        if (i == SchedulerType_Coroutine) {
            for (uint32 m = 0;m< m_u32CoroutineSchedulerCount;++m) {
                m_pSysSchedulerProcess[i][m].Init(SchedulerType_Coroutine);
                m_pSysSchedulerProcess[i][m].Start();
            }
        }
        if (i == SchedulerType_Synchronous) {
            for (uint32 m = 0; m < m_u32SyncSchedulerCount; ++m) {
                m_pSysSchedulerProcess[i][m].Init(SchedulerType_Synchronous);
                m_pSysSchedulerProcess[i][m].Start();
            }
        }
    }
}

xSchedulerMgr::~xSchedulerMgr() {

}

void xSchedulerMgr::AddScheduler(ServiceScheduler* pScheduler) {

    //XERR("xSchedulerMgr::AddScheduler \n");

    if (pScheduler->GetSchedulerStateType() == SchedulerType_Coroutine) {
        m_i32AddThreadCoroutineIndex++;
        m_i32AddThreadCoroutineIndex = m_i32AddThreadCoroutineIndex% m_u32CoroutineSchedulerCount;
        m_pSysSchedulerProcess[SchedulerType_Coroutine][m_i32AddThreadCoroutineIndex].AddScheduler(pScheduler);
    } else if(pScheduler->GetSchedulerStateType() == SchedulerType_Synchronous) {
        m_i32AddThreadSyncIndex++;
        m_i32AddThreadSyncIndex = m_i32AddThreadSyncIndex % m_u32SyncSchedulerCount;
        m_pSysSchedulerProcess[SchedulerType_Synchronous][m_i32AddThreadSyncIndex].AddScheduler(pScheduler);
    }
}

void xSchedulerMgr::Stop() {
    for (uint32 i = 0; i < SchedulerType_Max; ++i) {
        if (i == SchedulerType_Coroutine) {
            for (int m = 0; m < m_u32CoroutineSchedulerCount; ++m) {
                m_pSysSchedulerProcess[i][m].Stop();
            }
        }
        if (i == SchedulerType_Synchronous) {
            for (int m = 0; m < m_u32SyncSchedulerCount; ++m) {
                m_pSysSchedulerProcess[i][m].Stop();
            }
        }
    }

    for (uint32 i = 0; i < SchedulerType_Max; ++i) {
        delete[]m_pSysSchedulerProcess[i];
    }
    delete[]m_pSysSchedulerProcess;
    m_pSysSchedulerProcess = nullptr;
}


