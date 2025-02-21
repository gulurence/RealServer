#pragma once

#include "../xBase/xDefine.h"


// 调度器类型
enum SchedulerType
{
    // 协程调度
    SchedulerType_Coroutine = 0,
    // 同步调用
    SchedulerType_Synchronous,
    // 同步调用
    SchedulerType_Max,
};



// 调度状态
enum SchedulerStateType
{
    SchedulerStateType_Init = 0,
    // 当前没有event需要执行 等待event到来
    SchedulerStateType_Wait,
    // 当前有event需要执行
    SchedulerStateType_Ready,
    // event执行中
    SchedulerStateType_Running,
    // event执行阻塞中
    SchedulerStateType_Blocked,
    // event执行阻塞 结束
    SchedulerStateType_Blocked_End,
    // 重置调度中
    SchedulerStateType_Reset,
};































