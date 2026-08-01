#include "ActorMgr.h"
#include "ActorCreator.h"
#include "tService/tServiceMgr.h"
#include "xLog/xLog.h"

CActorMgr::CActorMgr() {
    XLOG("[INIT] CActorMgr created");
}

CActorMgr::~CActorMgr() {
    XLOG("[INIT] CActorMgr destroyed");
}

// -------------------------------------------------------------------
// Hosted Type Management
// -------------------------------------------------------------------

void CActorMgr::AddHostedType(ActorType i32Type) {
    std::lock_guard<std::mutex> lock(m_stLock);
    m_setHostedTypes.insert(i32Type);
    XLOG("[ActorMgr] Added hosted type: %d", i32Type);
}

bool CActorMgr::CanHostType(ActorType i32Type) const {
    std::lock_guard<std::mutex> lock(m_stLock);
    return m_setHostedTypes.find(i32Type) != m_setHostedTypes.end();
}

// -------------------------------------------------------------------
// Instance Management
// -------------------------------------------------------------------

bool CActorMgr::AddInstance(CActor* pActor) {
    if (!pActor) {
        XERR("[ActorMgr] AddInstance: null actor");
        return false;
    }

    std::lock_guard<std::mutex> lock(m_stLock);

    ActorType type = pActor->GetActorType();
    ActorID   id   = pActor->GetActorID();

    auto& innerMap = m_mapInstances[type];

    if (innerMap.find(id) != innerMap.end()) {
        XERR("[ActorMgr] AddInstance: actor already exists type=%d id=%lld", type, id);
        return false;
    }

    innerMap[id] = pActor;
    XLOG("[ActorMgr] AddInstance: type=%d id=%lld name=%s",
         type, id, pActor->GetActorName().c_str());
    return true;
}

CActor* CActorMgr::RemoveInstance(ActorType i32Type, ActorID i64ID) {
    std::lock_guard<std::mutex> lock(m_stLock);

    auto outerIt = m_mapInstances.find(i32Type);
    if (outerIt == m_mapInstances.end()) {
        return nullptr;
    }

    auto& innerMap = outerIt->second;
    auto innerIt = innerMap.find(i64ID);
    if (innerIt == innerMap.end()) {
        return nullptr;
    }

    CActor* pActor = innerIt->second;
    innerMap.erase(innerIt);

    // Clean up outer map if type has no more instances
    if (innerMap.empty()) {
        m_mapInstances.erase(outerIt);
    }

    XLOG("[ActorMgr] RemoveInstance: type=%d id=%lld", i32Type, i64ID);
    return pActor;
}

// -------------------------------------------------------------------
// Lifecycle Operations
// -------------------------------------------------------------------

bool CActorMgr::Install(ActorType i32Type, ActorID i64ID) {
    auto* pActor = Find(i32Type, i64ID);
    if (!pActor) {
        XERR("[ActorMgr] Install: actor not found type=%d id=%lld", i32Type, i64ID);
        return false;
    }

    // 委托 tServiceMgr 管理三方服务生命周期（首次创建, 后续复用）
    if (!tServiceMgr::getMe().AcquireServices(i32Type)) {
        XERR("[ActorMgr] Install: tServiceMgr::AcquireServices failed type=%d", i32Type);
        return false;
    }

    // 调用 actor 自身的 Install
    if (!pActor->Install()) {
        XERR("[ActorMgr] Install: actor Install() failed type=%d id=%lld name=%s",
             i32Type, i64ID, pActor->GetActorName().c_str());
        tServiceMgr::getMe().ReleaseServices(i32Type);
        return false;
    }

    XLOG("[ActorMgr] Install: ok type=%d id=%lld name=%s",
         i32Type, i64ID, pActor->GetActorName().c_str());
    return true;
}

bool CActorMgr::UnInstall(ActorType i32Type, ActorID i64ID) {
    auto* pActor = Find(i32Type, i64ID);
    if (!pActor) {
        XERR("[ActorMgr] UnInstall: actor not found type=%d id=%lld", i32Type, i64ID);
        return false;
    }

    // 调用 actor 自身的 UnInstall
    if (!pActor->UnInstall()) {
        XERR("[ActorMgr] UnInstall: actor UnInstall() failed type=%d id=%lld name=%s",
             i32Type, i64ID, pActor->GetActorName().c_str());
        return false;
    }

    // 从实例 map 移除
    RemoveInstance(i32Type, i64ID);

    // 委托 tServiceMgr 释放服务（引用计数递减 → 归零自动销毁）
    tServiceMgr::getMe().ReleaseServices(i32Type);

    XLOG("[ActorMgr] UnInstall: ok type=%d id=%lld name=%s",
         i32Type, i64ID, pActor->GetActorName().c_str());
    return true;
}

int32 CActorMgr::InstallAll(ActorType i32Type) {
    int32 count = 0;

    std::vector<CActor*> actors;
    {
        std::lock_guard<std::mutex> lock(m_stLock);
        auto outerIt = m_mapInstances.find(i32Type);
        if (outerIt == m_mapInstances.end()) {
            return 0;
        }
        for (const auto& pair : outerIt->second) {
            actors.push_back(pair.second);
        }
    }

    for (auto* pActor : actors) {
        if (Install(i32Type, pActor->GetActorID())) {
            count++;
        }
    }

    return count;
}

int32 CActorMgr::UnInstallAll(ActorType i32Type) {
    int32 count = 0;

    std::vector<CActor*> actors;
    {
        std::lock_guard<std::mutex> lock(m_stLock);
        auto outerIt = m_mapInstances.find(i32Type);
        if (outerIt == m_mapInstances.end()) {
            return 0;
        }
        for (const auto& pair : outerIt->second) {
            actors.push_back(pair.second);
        }
    }

    for (auto* pActor : actors) {
        if (UnInstall(i32Type, pActor->GetActorID())) {
            count++;
        }
    }

    return count;
}

// -------------------------------------------------------------------
// Query Operations
// -------------------------------------------------------------------

CActor* CActorMgr::Find(ActorType i32Type, ActorID i64ID) {
    std::lock_guard<std::mutex> lock(m_stLock);

    auto outerIt = m_mapInstances.find(i32Type);
    if (outerIt == m_mapInstances.end()) {
        return nullptr;
    }

    auto& innerMap = outerIt->second;
    auto innerIt = innerMap.find(i64ID);
    if (innerIt == innerMap.end()) {
        return nullptr;
    }

    return innerIt->second;
}

std::vector<CActor*> CActorMgr::GetByType(ActorType i32Type) {
    std::lock_guard<std::mutex> lock(m_stLock);

    std::vector<CActor*> result;

    auto outerIt = m_mapInstances.find(i32Type);
    if (outerIt == m_mapInstances.end()) {
        return result;
    }

    for (const auto& pair : outerIt->second) {
        result.push_back(pair.second);
    }

    return result;
}

size_t CActorMgr::GetInstanceCount() const {
    std::lock_guard<std::mutex> lock(m_stLock);

    size_t count = 0;
    for (const auto& outerPair : m_mapInstances) {
        count += outerPair.second.size();
    }
    return count;
}
