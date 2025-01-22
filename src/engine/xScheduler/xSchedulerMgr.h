#pragma once

#include "xSchedulerDefine.h"
#include "xSchedulerProcess.h"
#include "xScheduler.h"
#include "xBase/xCircularPool.h"
#include "xBase/xSingleton.h"



class xSchedulerMgr : public xSingleton<xSchedulerMgr>
{
public:
    xSchedulerMgr(uint32 u32CoroutineSchedulerCount=2, uint32 u32SyncSchedulerCount = 2);
    virtual ~xSchedulerMgr();

public:
    bool IsRunning() { return m_bIsRunning; }

public:
    void Stop();

public:
    void AddScheduler(ServiceScheduler* pScheduler);

private:
    // 用于轮询加入线程
    int m_i32AddThreadCoroutineIndex = 0;
    int m_i32AddThreadSyncIndex = 0;
    std::atomic<bool> m_bIsRunning = true;
    uint32 m_u32CoroutineSchedulerCount = 0;
    uint32 m_u32SyncSchedulerCount = 0;

private:
    xSchedulerProcess** m_pSysSchedulerProcess = nullptr;
};



