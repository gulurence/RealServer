
#include "xSchedulerProcess.h"
#include "xSchedulerMgr.h"

void RunLogic(xSchedulerProcess* pSchedulerNode) {
    auto &listData = pSchedulerNode->GetList();
    while (pSchedulerNode->IsRunning()) {
        ServiceScheduler* pScheduler = nullptr;
        if (listData.Pop(pScheduler)) {
            pScheduler->RunEvent();
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    }
}

bool xSchedulerProcess::Init(const SchedulerType& enType) {
    m_enType = enType;
    return true;
}

void xSchedulerProcess::AddScheduler(ServiceScheduler* pScheduler) {
    m_listServiceScheduler.Push(pScheduler);
}


xCircularPool<ServiceScheduler*>& xSchedulerProcess::GetList() {
    return m_listServiceScheduler;
}


void xSchedulerProcess::Start() {
    m_pThread = new std::thread(RunLogic, this);
}

void xSchedulerProcess::Stop() {
    m_bRunning = false;
    m_pThread->join();
    delete m_pThread;
    m_pThread = nullptr;
}

