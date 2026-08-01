#pragma once

#include "Actor.h"
#include "xBase/xSingleton.h"
#include <unordered_map>
#include <set>
#include <map>
#include <mutex>
#include <vector>

// CActorMgr is the central instance manager for all actors in a process.
// It tracks actor instances by (type, id) and provides Install/UnInstall/Find
// operations. Third-party service lifecycle (MySQL, Redis, Log, Xml, Platform)
// is delegated to tServiceMgr for reference-counted sharing across actors.
//
// Lifecycle:
//   1. RegisterActorDeps: declare which services each ActorType needs
//   2. Install(type, id): tServiceMgr::AcquireServices → create/reuse services
//      → call actor->Install()
//   3. UnInstall(type, id): actor->UnInstall() → tServiceMgr::ReleaseServices
//      → auto-destroy services when ref count reaches zero

class CActorMgr : public xSingleton<CActorMgr>
{
public:
    CActorMgr();
    ~CActorMgr();

    // --- Hosted Type Management ---
    void AddHostedType(ActorType i32Type);
    bool CanHostType(ActorType i32Type) const;
    const std::set<ActorType>& GetHostedTypes() const { return m_setHostedTypes; }

    // --- Instance Management ---
    bool AddInstance(CActor* pActor);
    CActor* RemoveInstance(ActorType i32Type, ActorID i64ID);

    // --- Lifecycle Operations ---
    // Install actor environment: delegates to tServiceMgr for shared services,
    // then calls actor->Install()
    bool Install(ActorType i32Type, ActorID i64ID);

    // Uninstall actor: calls actor->UnInstall(), removes from instances,
    // delegates service release to tServiceMgr
    bool UnInstall(ActorType i32Type, ActorID i64ID);

    // Install / UnInstall all actors of a type
    int32 InstallAll(ActorType i32Type);
    int32 UnInstallAll(ActorType i32Type);

    // --- Query Operations ---
    CActor* Find(ActorType i32Type, ActorID i64ID);
    std::vector<CActor*> GetByType(ActorType i32Type);
    size_t GetInstanceCount() const;

private:
    // type → (id → actor) two-level map
    std::unordered_map<ActorType, CActorMap> m_mapInstances;

    // Actor types this process is configured to host
    std::set<ActorType> m_setHostedTypes;

    mutable std::mutex m_stLock;
};
