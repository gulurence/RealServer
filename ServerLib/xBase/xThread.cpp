#include "xThread.h"
#include "xTools.h"

xThread::xThread()
{
    thread_setState(THREAD_INIT);
}

xThread::~xThread()
{
}

bool xThread::thread_start()
{
    /*
    using namespace std;
    pthread_attr_t attr;
    int policy;
    pthread_attr_init( &attr );
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    int rs = pthread_attr_setschedpolicy(&attr, SCHED_RR);
    struct sched_param param;
    int max_priority = sched_get_priority_max(SCHED_RR);
    param.sched_priority = max_priority;
#ifdef _LX_DEBUG
    XLOG("[线程],max_priority:%d rs:%d", param.sched_priority, rs);
#endif
    pthread_attr_setschedparam(&attr, &param);
    */

    if (!thread_init()) return false;

    pthread_t tidp;
    int ret = pthread_create(&tidp, NULL, &thread_run, (void *)this);
    //pthread_attr_destroy( &attr );
    if (ret==0)
    {
        //setState(THREAD_RUN);
    //    pthread_join(tidp,NULL);
        return true;
    }
    else
    {
        XERR("创建线程失败 err=%d", ret);
        return false;
    }
}

void xThread::thread_stop()
{
    while (thread_getState()!=THREAD_FINISH)
    {
        if (thread_getState()==THREAD_RUN)
            thread_setState(THREAD_STOP);
    }
}

void *xThread::thread_run(void *param)
{
    xThread *t = (xThread *)param;
    t->thread_proc();
    t->thread_setState(THREAD_FINISH);
    return 0;
}

