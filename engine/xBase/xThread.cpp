#include "xThread.h"
#include "xTools.h"


xThread::xThread() {
    thread_setState(THREAD_INIT);
}

xThread::~xThread() {
}

bool xThread::thread_start() {
    if (!thread_init()) {
        return false;
    }    
    std::thread t(thread_run, std::ref(*this));
    t.detach();
    m_threadId = t.get_id();
    return true;
}

void xThread::thread_stop() {
    while (thread_getState() != THREAD_FINISH) {
        if (thread_getState() == THREAD_RUN) {
            thread_setState(THREAD_STOP);
        }
    }
}

void *xThread::thread_run(xThread &param) {
    param.thread_proc();
    param.thread_setState(THREAD_FINISH);
    return 0;
}








