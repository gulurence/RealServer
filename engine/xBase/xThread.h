/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: xThread.h
      Purpose:   xThread 线程基类

      定义 xThread 抽象类与 ThreadState 状态枚举
      提供 thread_init/thread_proc 虚接口与 thread_run 静态回调
  *****************************************************************/

#pragma once

#include "xDefine.h"

class xThread
{
public:
    enum ThreadState
    {
        THREAD_INIT = 0, // after new, before start()
        THREAD_RUN,      // in while()
        THREAD_STOP,     // stopping
        THREAD_FINISH,   // can delete
    };

public:
    xThread();
    virtual ~xThread();

public:
    bool thread_start();

    virtual bool thread_init()
    {
        return true;
    }

    virtual void thread_stop();

    void thread_setState(ThreadState s)
    {
        thread_state = s;
    }
    ThreadState thread_getState()
    {
        return thread_state;
    }

    static void *thread_run(xThread &param);

protected:
    std::thread::id m_threadId;
    ThreadState thread_state;
    virtual void thread_proc() = 0;
};
