#pragma once

#include "tThridService.h"
#include "xBase/xSingleton.h"
#include "xBase/xDefine.h"

#include <map>
#include <set>
#include <mutex>

// Actor 依赖声明：每个 ActorType 需要哪些 ThridService
struct ActorDependency {
    ActorType actorType;
    std::set<EThridServiceType> requiredServices;
};

// =============================================================================
// tServiceMgr — 统一第三方服务管理器 (Singleton)
//
// 职责：
//   1. 管理所有 ThridService 实例的创建 / 复用 / 销毁
//   2. Actor Install 时根据依赖声明自动获取服务（共享复用 + 引用计数）
//   3. Actor UnInstall 时释放服务（引用计数递减 → 归零销毁）
//
// 使用方式：
//   // 启动时注册依赖
//   tServiceMgr::RegisterActorDeps(100, {EST_Redis, EST_Log, EST_Xml});
//
//   // Actor Install 时
//   tServiceMgr::AcquireServices(actorType); // 自动创建或复用
//
//   // Actor UnInstall 时
//   tServiceMgr::ReleaseServices(actorType); // 引用计数递减，归零销毁
// =============================================================================
class tServiceMgr : public xSingleton<tServiceMgr>
{
public:
    tServiceMgr();
    ~tServiceMgr();

    // ---- Actor 依赖注册 ----
    // 声明某个 Actor 类型需要哪些三方服务（在 Actor 加载/注册时调用）
    void RegisterActorDeps(ActorType actorType,
                           const std::set<EThridServiceType>& services);

    // 查询某 Actor 类型的依赖
    const std::set<EThridServiceType>* GetActorDeps(ActorType actorType) const;

    // ---- 服务生命周期 ----
    // 为 Actor Install 准备所需服务（检查已存在 → 复用或创建）
    // 每个 ActorType 每调用一次，该类型的引用计数 +1
    bool AcquireServices(ActorType actorType);

    // Actor UnInstall 时释放（引用计数递减 → 归零时销毁所有依赖服务）
    void ReleaseServices(ActorType actorType);

    // ---- 查询 ----
    ThridService* GetService(EThridServiceType enType);
    int32 GetServiceRefCount(EThridServiceType enType) const;

    // ---- 配置 ----
    // 设置某类型服务的配置（在 AcquireServices 首次创建时使用）
    void SetServiceConfig(EThridServiceType enType, const ThridServiceConfig& cfg);

private:
    // 创建具体 ThridService 子类实例（工厂方法）
    ThridService* CreateService(EThridServiceType enType);

    // 销毁服务实例
    void DestroyService(EThridServiceType enType);

    // ---- 状态 ----
    // type → ThridService* (每个类型最多一个共享实例)
    std::map<EThridServiceType, ThridService*> m_mapServices;

    // type → 配置（启动时从 server.xml 填入）
    std::map<EThridServiceType, ThridServiceConfig> m_mapConfigs;

    // ActorType → 依赖声明
    std::map<ActorType, ActorDependency> m_mapActorDeps;

    // ActorType → 当前有多少个实例在使用服务
    std::map<ActorType, int32> m_mapActorRefCounts;

    // 服务 ID 自增
    int32 m_i32NextServiceID = 1;

    mutable std::mutex m_stLock;
};
