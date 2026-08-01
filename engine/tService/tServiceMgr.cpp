#include "tServiceMgr.h"
#include "tServiceMySql.h"
#include "tServiceRedis.h"
#include "tServiceLog.h"
#include "tServiceXml.h"
#include "tServicePlatformMgr.h"
#include "xLog/xLog.h"

tServiceMgr::tServiceMgr()  = default;
tServiceMgr::~tServiceMgr() = default;

// ---------------------------------------------------------------------------
// Actor 依赖注册
// ---------------------------------------------------------------------------

void tServiceMgr::RegisterActorDeps(ActorType actorType,
                                     const std::set<EThridServiceType>& services) {
    std::lock_guard<std::mutex> lock(m_stLock);

    ActorDependency dep;
    dep.actorType        = actorType;
    dep.requiredServices = services;
    m_mapActorDeps[actorType] = dep;
    m_mapActorRefCounts[actorType] = 0;

    XLOG("[tServiceMgr] Registered deps for ActorType=%d (services=%zu)",
         actorType, services.size());
}

const std::set<EThridServiceType>* tServiceMgr::GetActorDeps(
    ActorType actorType) const {
    std::lock_guard<std::mutex> lock(m_stLock);
    auto it = m_mapActorDeps.find(actorType);
    if (it != m_mapActorDeps.end()) {
        return &it->second.requiredServices;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// 服务生命周期
// ---------------------------------------------------------------------------

bool tServiceMgr::AcquireServices(ActorType actorType) {
    std::lock_guard<std::mutex> lock(m_stLock);

    // 1. 查找依赖声明
    auto depIt = m_mapActorDeps.find(actorType);
    if (depIt == m_mapActorDeps.end()) {
        XERR("[tServiceMgr] AcquireServices: no deps registered for ActorType=%d",
             actorType);
        return false;
    }

    const auto& required = depIt->second.requiredServices;

    // 2. 遍历依赖 — 检查已存在则复用，否则创建
    for (EThridServiceType svcType : required) {
        auto svcIt = m_mapServices.find(svcType);

        if (svcIt != m_mapServices.end() && svcIt->second) {
            // 已存在 → 复用，引用计数 +1
            svcIt->second->AddRef();
            XLOG("[tServiceMgr] Reuse service type=%d (ref=%d) for ActorType=%d",
                 (int)svcType, svcIt->second->GetRefCount(), actorType);
        } else {
            // 不存在 → 创建新实例
            ThridService* pSvc = CreateService(svcType);
            if (!pSvc) {
                XERR("[tServiceMgr] AcquireServices: create failed type=%d for ActorType=%d",
                     (int)svcType, actorType);
                // 回滚已创建的
                return false;
            }

            // 获取配置并初始化
            auto cfgIt = m_mapConfigs.find(svcType);
            if (cfgIt != m_mapConfigs.end()) {
                if (!pSvc->Init(cfgIt->second)) {
                    XERR("[tServiceMgr] AcquireServices: init failed type=%d", (int)svcType);
                    delete pSvc;
                    return false;
                }
            }

            pSvc->AddRef();
            m_mapServices[svcType] = pSvc;
            XLOG("[tServiceMgr] Created service type=%d name=%s (ref=1) for ActorType=%d",
                 (int)svcType, pSvc->GetName().c_str(), actorType);
        }
    }

    // 3. ActorType 引用计数 +1
    m_mapActorRefCounts[actorType]++;
    XLOG("[tServiceMgr] AcquireServices ok: ActorType=%d (actorRef=%d)",
         actorType, m_mapActorRefCounts[actorType]);

    return true;
}

void tServiceMgr::ReleaseServices(ActorType actorType) {
    std::lock_guard<std::mutex> lock(m_stLock);

    auto refIt = m_mapActorRefCounts.find(actorType);
    if (refIt == m_mapActorRefCounts.end()) {
        XWRN("[tServiceMgr] ReleaseServices: no ref count for ActorType=%d", actorType);
        return;
    }

    refIt->second--;
    int32 remaining = refIt->second;

    if (remaining > 0) {
        // 还有同类型 Actor 在使用 → 只减计数
        XLOG("[tServiceMgr] ReleaseServices: ActorType=%d ref decreased (remaining=%d)",
             actorType, remaining);
        return;
    }

    // 引用计数归零 → 释放所有依赖服务
    XLOG("[tServiceMgr] ReleaseServices: ActorType=%d ref=0, releasing services...", actorType);

    auto depIt = m_mapActorDeps.find(actorType);
    if (depIt == m_mapActorDeps.end()) {
        m_mapActorRefCounts.erase(refIt);
        return;
    }

    for (EThridServiceType svcType : depIt->second.requiredServices) {
        auto svcIt = m_mapServices.find(svcType);
        if (svcIt == m_mapServices.end() || !svcIt->second) continue;

        int32 newRef = svcIt->second->DecRef();
        XLOG("[tServiceMgr] Service type=%d ref decreased to %d", (int)svcType, newRef);

        if (newRef <= 0) {
            DestroyService(svcType);
        }
    }

    m_mapActorRefCounts.erase(refIt);
}

// ---------------------------------------------------------------------------
// 查询
// ---------------------------------------------------------------------------

ThridService* tServiceMgr::GetService(EThridServiceType enType) {
    std::lock_guard<std::mutex> lock(m_stLock);
    auto it = m_mapServices.find(enType);
    if (it != m_mapServices.end()) return it->second;
    return nullptr;
}

int32 tServiceMgr::GetServiceRefCount(EThridServiceType enType) const {
    std::lock_guard<std::mutex> lock(m_stLock);
    auto it = m_mapServices.find(enType);
    if (it != m_mapServices.end() && it->second) {
        return it->second->GetRefCount();
    }
    return 0;
}

// ---------------------------------------------------------------------------
// 配置
// ---------------------------------------------------------------------------

void tServiceMgr::SetServiceConfig(EThridServiceType enType,
                                    const ThridServiceConfig& cfg) {
    std::lock_guard<std::mutex> lock(m_stLock);
    m_mapConfigs[enType] = cfg;
    XLOG("[INIT] tServiceMgr config set for service type %d", (int)enType);
}

// ---------------------------------------------------------------------------
// 工厂方法 — 创建具体 ThridService 子类
// ---------------------------------------------------------------------------

ThridService* tServiceMgr::CreateService(EThridServiceType enType) {
    int32 id = m_i32NextServiceID++;
    ThridService* pSvc = nullptr;

    switch (enType) {
    case EST_MySql:
        pSvc = new tServiceMySql(id, "mysql_svc");
        break;
    case EST_Redis:
        pSvc = new tServiceRedis(id, "redis_svc");
        break;
    case EST_Log:
        pSvc = new tServiceLog(id, "log_svc");
        break;
    case EST_Xml:
        pSvc = new tServiceXml(id, "xml_svc");
        break;
    case EST_PlatformMgr:
        pSvc = new tServicePlatformMgr(id, "platform_svc");
        break;
    default:
        XERR("[tServiceMgr] CreateService: unknown type=%d", (int)enType);
        return nullptr;
    }

    return pSvc;
}

void tServiceMgr::DestroyService(EThridServiceType enType) {
    auto it = m_mapServices.find(enType);
    if (it == m_mapServices.end()) return;

    ThridService* pSvc = it->second;
    XLOG("[tServiceMgr] Destroying service type=%d name=%s",
         (int)enType, pSvc->GetName().c_str());

    pSvc->Release();
    delete pSvc;
    m_mapServices.erase(it);
}
