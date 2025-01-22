#pragma once

#include "xSchedulerDefine.h"

#include "xScheduler.h"

#include "xBase/xCircularPool.h"


// 同步异步线程进行ServiceEvent调度
class xSchedulerProcess
{
public:
    xSchedulerProcess() {}
    ~xSchedulerProcess() {}

public:
    bool Init(const SchedulerType &enType);
    void Start();
    void Stop();

public:
    SchedulerType GetSchedulerType() { return m_enType; }
    bool IsRunning() { return m_bRunning; }

public:
    void AddScheduler(ServiceScheduler* pScheduler);

public:
    xCircularPool<ServiceScheduler*>& GetList();

private:
    // 调度器类型
    SchedulerType m_enType;
    std::atomic<bool> m_bRunning = true;
    // 当前调度中的列表
    xCircularPool<ServiceScheduler*> m_listServiceScheduler;

private:
    std::thread* m_pThread = nullptr;
};

