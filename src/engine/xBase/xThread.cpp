#include "xThread.h"
#include "xTools.h"


xThread::xThread() {
    thread_setState(THREAD_INIT);
}

xThread::~xThread() {
}

bool xThread::thread_start() {

    if (!thread_init()) 
        return false;
    
    // 闁告帗绋戠紓鎾存交閹邦垼鏀界紒鎹愭硶閳?
    std::thread t(thread_run, std::ref(*this));

    // 闁告艾楠歌ぐ瀛樻交閹邦垼鏀?
    t.detach();

    // 闁兼儳鍢茶ぐ鍥╃棯鐠恒劉鏌d
    m_threadId = t.get_id();

    return true;
}

void xThread::thread_stop() {
    while (thread_getState() != THREAD_FINISH) {
        if (thread_getState() == THREAD_RUN)
            thread_setState(THREAD_STOP);
    }
}

void *xThread::thread_run(xThread &param) {
    param.thread_proc();
    param.thread_setState(THREAD_FINISH);
    return 0;
}








