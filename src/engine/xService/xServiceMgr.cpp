
#include "xServiceMgr.h"


void RunLogic(xServiceMgr *pxServiceMgr) {
    while (true) {
        {
            std::lock_guard l(pxServiceMgr->m_stLock);

            for (auto &it: pxServiceMgr->GetServices()) {
                auto pService = it.second;
                auto pScheduler = pService->GetServiceScheduler();
                pScheduler->processRequests(pScheduler.get());
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

xServiceMgr::xServiceMgr() {

    std::thread t(RunLogic,this);
    t.detach();
    //
    //XINF("xServiceMgr::xServiceMgr Create [%s]", pszServiceName);
}

xServiceMgr::~xServiceMgr() {

}

void xServiceMgr::Release() {

}


bool xServiceMgr::Regist(xService* pService) {
    std::lock_guard l(m_stLock);
    auto it = m_mapService.find(pService->Id());
    if (it != m_mapService.end()) {
        XERR("double regist service [%ld]", pService->Id());
        return false;
    }
    m_mapService[pService->Id()] = pService;
    return true;
}

xService* xServiceMgr::Get(const ServiceID& u64ServiceID) {
    std::lock_guard l(m_stLock);
    auto it = m_mapService.find(u64ServiceID);
    if (it == m_mapService.end()) {
        XERR("not find service [%ld]", u64ServiceID);
        return nullptr;
    }
    return it->second;
}

bool xServiceMgr::UnRegist() {
    return true;
}
