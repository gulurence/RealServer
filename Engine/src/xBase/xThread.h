#ifndef _X_THREAD_H_
#define _X_THREAD_H_

#include "StdInc.h"

class xThread
{
public:
    enum ThreadState
    {
        THREAD_INIT = 0,//after new, before start()
        THREAD_RUN,//in while()
        THREAD_STOP,//stopping
        THREAD_FINISH,//can delete
    };

public:
    xThread();
    virtual ~xThread();

public:
    bool thread_start();

    virtual bool thread_init() {
        return true;
    }

    virtual void thread_stop();

    void thread_setState(ThreadState s) {
        thread_state = s;
    }
    ThreadState thread_getState() {
        return thread_state;
    }

    static void *thread_run(xThread &param);

protected:
    thread::id m_threadId;
    ThreadState thread_state;
    virtual void thread_proc() = 0;
};


#endif // end _X_THREAD_H_
