
#include "xServiceMgr.h"
#include "xScheduler/xScheduler.h"


xServiceMgr::xServiceMgr() {

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
