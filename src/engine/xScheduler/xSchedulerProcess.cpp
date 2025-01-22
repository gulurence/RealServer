
#include "xSchedulerProcess.h"
#include "xSchedulerMgr.h"

void RunLogic(xSchedulerProcess* pSchedulerNode) {
    while (pSchedulerNode->IsRunning()) {
        auto &list = pSchedulerNode->GetList();
        ServiceScheduler* pScheduler = nullptr;
        if (list.Pop(pScheduler)) {
            pScheduler->RunEvent();
        }
        //{
        //    for (auto &it: pxServiceMgr->GetServices()) {
        //        auto pService = it.second;
        //        auto pScheduler = pService->GetServiceScheduler();
        //        if (pScheduler->GetSchedulerState() == ServiceSchedulerRunning) {
        //            pScheduler->processRequests(pScheduler.get());
        //        }
        //    }
        //}
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
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

