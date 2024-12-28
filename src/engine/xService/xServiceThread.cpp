
#include "xServiceThread.h"

#include "xLog/xLog.h"


xServiceThread::xServiceThread(uint16 u16ThreadIndex):m_u16ThreadIndex(u16ThreadIndex){

}

xServiceThread::~xServiceThread() {

}

void xServiceThread::thread_proc() {




}





xServiceThreadMgr::xServiceThreadMgr(uint16 u16ThreadCount) {
    m_u16ThreadCount = u16ThreadCount;

    if (m_u16ThreadCount <= 0 ) {
        m_u16ThreadCount = 2;
    }else if ( m_u16ThreadCount > 8) {
        m_u16ThreadCount = 8;
    }

    XINF("xServiceThreadMgr Create ThreadCount:%d ", m_u16ThreadCount);

    for (int i = 0; i < m_u16ThreadCount; ++i) {
        m_listServiceThread.push_back(new xServiceThread(i + 1));
    }
}

xServiceThreadMgr::~xServiceThreadMgr() {

}

void xServiceThreadMgr::Start() {
    for (auto &it: m_listServiceThread) {
        xServiceThread* pServiceThread = it;
        pServiceThread->thread_start();
    }
}

void xServiceThreadMgr::Pause() {

}

void xServiceThreadMgr::Stop() {
    for (auto& it : m_listServiceThread) {
        xServiceThread* pServiceThread = it;
        pServiceThread->thread_stop();
    }
}

