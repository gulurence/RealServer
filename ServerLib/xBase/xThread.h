#ifndef _X_THREAD_H_
#define _X_THREAD_H_

#ifdef _WINDOWS
#include "pthread.h"
#endif

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

    xThread();
    virtual ~xThread();

    bool thread_start();
    virtual bool thread_init(){return true;}
    virtual void thread_stop();
    void thread_setState(ThreadState s){thread_state = s;}
    ThreadState thread_getState(){return thread_state;}

    static void *thread_run(void *param);
protected:
    ThreadState thread_state;
    virtual void thread_proc()=0;
};


#endif // end _X_THREAD_H_
