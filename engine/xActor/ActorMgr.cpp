/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: ActorMgr.cpp
      Purpose:   进程级 Actor 实例管理器实现

      按 type 到 (id 到 actor) 两级索引维护实例注册表, 提供 AddInstance/RemoveInstance/Find/FindByActorID/GetByType
      Install/UnInstall 多步骤生命周期: tServiceMgr 服务引用计数、Player 路由索引、Redis 注册、状态快照恢复、消息循环挂载
      实例增删即时 FlushLoad 回写 Redis 负载, 避免并发建 Actor 扎堆选同一低负载节点
  *****************************************************************/

#include "ActorMgr.h"
#include "ActorCreator.h"
#include "ActorScheduler.h"
#include "tService/tServiceMgr.h"
#include "xCluster/xProcessDefine.h"    // EActorType_APlayer 等枚举
#include "xCluster/CPlayerActorIndex.h" // player:{id} → actorId 路由索引
#include "xCluster/xServiceDiscovery.h" // 实例增删时即时回写负载 (FlushLoad)
#include "xGm/GmCommandMgr.h"           // Actor 实例注册 (K8s 风格管理)
#include "xLog/xLog.h"

// ============================================================================
// CActorMgr — 进程级 Actor 实例管理器
// ============================================================================
//
// 职责概述：
//   CActorMgr 维护本进程内所有 Actor 实例的运行时注册表，按 (类型, ID) 双级
//   索引存储，并提供 Install / UnInstall / Find 等核心操作。
//
// 关键设计点：
//   1. 双级索引结构
//      m_mapInstances: EActorType → (ActorID → CActor*)
//      外层 unordered_map 按类型快速定位，内层 map 按 ID 排序。
//
//   2. 三方服务引用计数（tServiceMgr）
//      MySQL、Redis、Log、Xml、Platform 等三方服务由 tServiceMgr 按类型统一
//      管理。同一 EActorType 的多个实例共享同一套服务连接。
//      - Install 时 AcquireServices（引用计数 +1，首次则创建）
//      - UnInstall 时 ReleaseServices（引用计数 -1，归零则自动销毁）
//
//   3. 线程安全
//      所有公共方法通过 m_stLock（std::mutex）保护，允许从多个线程并发调用。
//      Install / UnInstall 过程中，CActor 虚函数回调（Install/UnInstall）发生
//      在锁外——因为这期间结构不会变，锁内只做 map 查找和插入。
//
//   4. 原型与实例
//      - ActorID == 0 表示原型（模板），用于同类型 Actor 的工厂创建
//      - ActorID != 0 表示具体实例
//      - 两者都存储在 m_mapInstances 中，遵循相同的生命周期
//
// 生命周期流程：
//
//   注册阶段（进程启动）:
//     registerPrototype (Lua)
//       → si_lua_actor_create(type, 0, name)    -- 创建 C++ 原型 Actor
//       → si_actor_regist(type, proto)           -- 注册到 ActorCreator 工厂
//       → si_actor_mgr_add_instance(proto)       -- 加入实例管理器 ← 本类
//       → si_actor_mgr_install(type, 0)          -- 安装原型环境
//
//   运行阶段（创建实例）:
//     createLocal (Lua)
//       → si_actor_create(type, id)              -- 从原型克隆出新实例
//       → si_actor_mgr_add_instance(actor)       -- 加入实例管理器 ← 本类
//       → si_actor_mgr_install(type, id)         -- 安装运行环境
//       → actor->data_load() / onInit()          -- 加载数据 / 业务初始化
//
//   销毁阶段:
//     deleteLocal (Lua)
//       → actor->data_save(3)                    -- 保存数据
//       → si_actor_mgr_uninstall(type, id)       -- 卸载运行环境
//       → si_actor_creator_delete(type, id)      -- 销毁 C++ Actor
//
// ============================================================================

// ---------------------------------------------------------------------------
// 构造 / 析构
// ---------------------------------------------------------------------------

CActorMgr::CActorMgr()
{
    XLOG("[ActorMgr] Constructor — instance manager created");
}

CActorMgr::~CActorMgr()
{
    // 析构时打印残留统计（正常情况下应该已被清空）
    std::lock_guard<std::mutex> lock(m_stLock);
    size_t totalInstances = 0;
    for (const auto &outerPair : m_mapInstances)
    {
        totalInstances += outerPair.second.size();
    }
    XLOG("[ActorMgr] Destructor — hosted_types=%zu residual_instances=%zu", m_setHostedTypes.size(),
         totalInstances);

    // 注：析构时不主动调用 UnInstall。
    // 进程关闭时由框架按类型批量 UnInstallAll，此处仅做清理提示。
}

// ============================================================================
// Hosted Type Management（托管类型管理）
// ============================================================================

// AddHostedType — 声明本进程可托管指定类型的 Actor
//
// 此函数在进程启动时由 Lua 侧调用（xEngine.ActorMgr.hostType），
// 声明"本进程愿意承载类型为 actorType 的 Actor 实例"。
// 调用后：
//   - CanHostType(type) 返回 true
//   - 负载均衡器可将该类型的 Actor 调度到本进程
//
// 幂等：重复调用同一类型仅记录一次。
void CActorMgr::AddHostedType(EActorType i32Type)
{
    std::lock_guard<std::mutex> lock(m_stLock);

    // 检查是否已存在（幂等保护）
    if (m_setHostedTypes.find(i32Type) != m_setHostedTypes.end())
    {
        XLOG("[ActorMgr] AddHostedType: type=%d already registered, skip", i32Type);
        return;
    }

    m_setHostedTypes.insert(i32Type);
    XLOG("[ActorMgr] AddHostedType: type=%d accepted — process can now host this type", i32Type);
}

// CanHostType — 查询本进程是否能托管指定类型的 Actor
//
// 用于负载均衡器询问"这个进程能不能接 type=X 的 Actor"。
// 纯只读查询，不修改任何状态。
bool CActorMgr::CanHostType(EActorType i32Type) const
{
    std::lock_guard<std::mutex> lock(m_stLock);

    bool canHost = (m_setHostedTypes.find(i32Type) != m_setHostedTypes.end());
    // 高频调用，仅在结果为 false 时打日志（正常情况无需记录）
    if (!canHost)
    {
        XWRN("[ActorMgr] CanHostType: type=%d NOT hosted here", i32Type);
    }
    return canHost;
}

// ============================================================================
// Instance Management（实例增删）
// ============================================================================

// AddInstance — 将 Actor 指针加入实例注册表
//
// 将 CActor* 存入 m_mapInstances[type][id]。
// 调用时机：
//   - registerPrototype: 原型（id=0）创建后
//   - createLocal: 实例创建后，install 之前
//
// 约束：
//   - pointer 不能为空
//   - 同一 (type, id) 不能重复添加（重复调用说明逻辑错误）
//
// 线程安全：加锁保护整个写入过程。
bool CActorMgr::AddInstance(CActor *pActor)
{
    // --- 空指针检查 ---
    // 调用方传递了 nullptr，通常说明 si_actor_create / si_lua_actor_create
    // 失败，Lua 侧应在调用本函数前先检查返回值。
    if (!pActor)
    {
        XERR("[ActorMgr] AddInstance: FAIL — null actor pointer");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_stLock);

        // --- 提取 key 信息 ---
        // GetActorType() / GetActorID() 是 CActor 虚函数，返回注册时设定的值。
        EActorType type = pActor->GetActorType();
        ActorID id = pActor->GetActorID();

        XLOG("[ActorMgr] AddInstance: requesting type=%d id=%lld name=\"%s\"", type, id,
             pActor->GetActorName().c_str());

        // --- 获取或创建内层 map ---
        // m_mapInstances 是 unordered_map<EActorType, map<ActorID, CActor*>>
        // operator[] 对 key=type 不存在时自动创建空的 inner map。
        auto &innerMap = m_mapInstances[type];

        // --- 重复检查 ---
        // 同一 (type, id) 不能有两个 CActor 对象，说明上层逻辑有误。
        if (innerMap.find(id) != innerMap.end())
        {
            XERR("[ActorMgr] AddInstance: FAIL — duplicate type=%d id=%lld "
                 "(existing name=\"%s\", attempted name=\"%s\")",
                 type, id, innerMap[id]->GetActorName().c_str(), pActor->GetActorName().c_str());
            return false;
        }

        // --- 插入 ---
        innerMap[id] = pActor;

        // --- 统计当前该类型总数 ---
        size_t typeCount = innerMap.size();

        XLOG("[ActorMgr] AddInstance: OK — type=%d id=%lld name=\"%s\" "
             "(type_count=%zu, total_types=%zu)",
             type, id, pActor->GetActorName().c_str(), typeCount, m_mapInstances.size());
    } // 释放 m_stLock

    // 实例数变化即时回写 Redis 负载: 若不即时, 负载均衡每次被动读到的都是 5s 心跳
    // 才刷新的陈旧 load, 并发建 Actor (登录/开局风暴) 会扎堆选中同一低负载节点。
    CServiceDiscovery::getMe().FlushLoad((int32)GetInstanceCount());
    return true;
}

// RemoveInstance — 从实例注册表中移除 Actor 并返回其指针
//
// 仅从 map 中删除索引条目，不调用 delete（销毁由调用方负责）。
// 调用时机：
//   - UnInstall 流程中（UnInstall 会自动调用 RemoveInstance）
//   - 直接调用（罕见，调用方需自行管理 Actor 生命周期）
//
// 返回值：
//   - 找到并移除：返回 CActor*（调用方获得所有权）
//   - 未找到：返回 nullptr
//
// 注意：当某类型的最后一个实例被移除时，外层的 type→map 条目也会被清除，
//       以保持 m_mapInstances 的整洁（后续查找不存在类型时直接返回空）。
CActor *CActorMgr::RemoveInstance(EActorType i32Type, ActorID i64ID)
{
    CActor *pActor = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_stLock);

        XLOG("[ActorMgr] RemoveInstance: requesting type=%d id=%lld", i32Type, i64ID);

        // --- 查找外层 map（按类型）---
        auto outerIt = m_mapInstances.find(i32Type);
        if (outerIt == m_mapInstances.end())
        {
            XWRN("[ActorMgr] RemoveInstance: NOT FOUND — type=%d has no entries at all", i32Type);
            return nullptr;
        }

        // --- 查找内层 map（按 ID）---
        auto &innerMap = outerIt->second;
        auto innerIt = innerMap.find(i64ID);
        if (innerIt == innerMap.end())
        {
            XWRN("[ActorMgr] RemoveInstance: NOT FOUND — type=%d id=%lld "
                 "(type has %zu entries, but id not among them)",
                 i32Type, i64ID, innerMap.size());
            return nullptr;
        }

        // --- 移除条目 ---
        pActor = innerIt->second;
        innerMap.erase(innerIt);

        XLOG("[ActorMgr] RemoveInstance: erased type=%d id=%lld name=\"%s\" "
             "(type_count=%zu→%zu)",
             i32Type, i64ID, pActor->GetActorName().c_str(), innerMap.size() + 1, innerMap.size());

        // --- 外键清理 ---
        // 如果该类型下已无任何实例，删除外层条目以保持结构整洁。
        // 这样后续 Find/GetByType 等查询可以直接从外层不存在判断。
        if (innerMap.empty())
        {
            m_mapInstances.erase(outerIt);
            XLOG("[ActorMgr] RemoveInstance: type=%d outer map removed — no instances remain "
                 "(total_types=%zu→%zu)",
                 i32Type, m_mapInstances.size() + 1, m_mapInstances.size());
        }
    } // 释放 m_stLock

    // 实例数变化即时回写 Redis 负载 (同 AddInstance, 释放槽位即时可见)。
    CServiceDiscovery::getMe().FlushLoad((int32)GetInstanceCount());
    return pActor;
}

// ============================================================================
// Lifecycle Operations（生命周期操作）
// ============================================================================

// Install — 安装 Actor 运行环境
//
// 一个 Actor 在可以执行业务逻辑之前，必须先完成 Install。
// Install 分为三个步骤，任一失败则回滚：
//
//   Step 1: Find — 在实例注册表中查找 Actor
//     Actor 必须已通过 AddInstance 注册，否则无法 Install。
//
//   Step 2: tServiceMgr::AcquireServices — 获取三方服务
//     为该 EActorType 获取 MySQL / Redis / Log / Xml / Platform 等服务。
//     同一类型的多个实例共享同一套服务（引用计数管理）。
//     - 首次 Acquire：创建新服务连接
//     - 后续 Acquire：ref_count++，复用已有连接
//
//   Step 3: actor->Install() — Actor 自身的安装逻辑
//     CActor 虚函数，由各 Actor 子类覆写。
//     典型操作：注册定时器、订阅消息、建立 RPC 监听等。
//     如果这一步失败，需要回滚 Step 2 获取的服务。
//
// 返回值：
//   true  — 全部三步成功，Actor 处于 "已安装" 状态
//   false — 任一步失败（Actor 不存在 / 服务获取失败 / 业务 Install 失败）
bool CActorMgr::Install(EActorType i32Type, ActorID i64ID)
{
    XLOG("[ActorMgr] Install: BEGIN type=%d id=%lld", i32Type, i64ID);

    // ===================================================================
    // Step 1: Find actor in registry
    // ===================================================================
    // 在锁内查找并获取裸指针。
    // 注意：Find 内部持有锁，返回后锁释放。
    //       这里设计上假设 actor 一旦注册就不会在 Install/UnInstall 期间被
    //       并发删除——调用方（Lua ActorMgr）保证顺序调用。
    auto *pActor = Find(i32Type, i64ID);
    if (!pActor)
    {
        XERR("[ActorMgr] Install: FAIL — actor not found type=%d id=%lld "
             "(was AddInstance called before Install?)",
             i32Type, i64ID);
        return false;
    }

    XLOG("[ActorMgr] Install: Step 1 OK — found actor name=\"%s\"", pActor->GetActorName().c_str());

    // ===================================================================
    // Step 2: Acquire shared third-party services via tServiceMgr
    // ===================================================================
    //
    // tServiceMgr 按 EActorType 维护服务引用计数。
    // 例如：Gate 类型(type=100) 需要 MySQL + Redis + RPC：
    //   - tServiceMgr::getMe().AcquireServices(100)
    //     → 检查该类型是否已注册依赖（RegisterActorDeps 已执行）
    //     → MySQL:   ref_count 0→1, 创建新连接池
    //     → Redis:   ref_count 0→1, 创建新连接池
    //     → RPC:     ref_count 0→1, 注册 gRPC 服务
    //   - 同一进程再次创建 Gate 实例时，ref_count 1→2，直接复用。
    //
    if (!tServiceMgr::getMe().AcquireServices(i32Type))
    {
        XERR("[ActorMgr] Install: FAIL — tServiceMgr::AcquireServices failed type=%d "
             "(check deps registration for this type, or service connection config)",
             i32Type);
        return false;
    }

    XLOG("[ActorMgr] Install: Step 2 OK — services acquired for type=%d", i32Type);

    // ===================================================================
    // Step 3: Call actor's own Install() virtual method
    // ===================================================================
    //
    // CActor::Install() 是纯虚函数 → CLuaActor::Install() 回调 Lua。
    // Lua 侧通常会执行：
    //   - 注册网络消息回调
    //   - 注册 RPC 处理器
    //   - 启动定时任务
    //   - 初始化业务状态
    //
    // 如果失败，必须回滚 Step 2 获取的服务，否则服务引用计数泄露。
    if (!pActor->Install())
    {
        XERR("[ActorMgr] Install: FAIL — actor->Install() returned false type=%d id=%lld "
             "name=\"%s\"",
             i32Type, i64ID, pActor->GetActorName().c_str());

        // --- 回滚 Step 2 ---
        // ReleaseServices 将引用计数 -1；如果这是该类型的最后一个引用，
        // tServiceMgr 会自动销毁服务连接。
        tServiceMgr::getMe().ReleaseServices(i32Type);
        XLOG("[ActorMgr] Install: rolled back — released services for type=%d", i32Type);
        return false;
    }

    XLOG("[ActorMgr] Install: OK — type=%d id=%lld name=\"%s\" (all 3 steps passed)", i32Type,
         i64ID, pActor->GetActorName().c_str());

    // ===================================================================
    // Step 4: Player 实例注册路由索引 player:{playerId} → actorId (M0)
    //
    // 说明: 当前阶段全局自描述 ActorID (MachineID+PID+ActorIndex) 尚未接入
    // 创建链路, 实例 ID 仍沿用业务 id (roleId), 故此处 playerId == actorId。
    // 待全局 ID 生成接入后, 此处改为:
    //     CPlayerActorIndex::instance()->Register(
    //         i64ID, xActorId::MakeActorId(machineId, pid, ++actorIndex));
    // 原型 (id=0) 与单例不参与注册。
    // ===================================================================
    if (i32Type == EActorType_APlayer && i64ID != 0)
    {
        CPlayerActorIndex::instance()->Register((int64)i64ID, (uint64)i64ID);
        // PlayerActor 总数 +1 (登录排队限流数据源, Login 进程据此判定是否排队)
        CPlayerActorIndex::instance()->IncrPlayerCount();
    }

    // ===================================================================
    // Step 5: Actor 实例注册到 Redis (K8s 风格框架管理)
    //
    // 写 cluster:actor:{type}:{id} + cluster:actor:by_type:{type}, 供 WebPlatform
    // actors.php 查看本进程承载的全部 Actor (含 id=0 的原型/单例)。Redis 不可用时
    // CGmCommandMgr::RegisterActor 内部安全降级 (仅告警), 不阻断 Install。
    // ===================================================================
    CGmCommandMgr::instance()->RegisterActor(i32Type, i64ID, pActor->GetActorName(),
                                             pActor->GetGlobalId());

    // ===================================================================
    // Step 6: 动态恢复 — 尝试从 actor:state:{type}:{id} 回填 Actor 状态 (P7)
    //
    // 解决 Actor 下线/进程宕机后重新拉起时的状态恢复: 若 Redis 存在该 Actor 的
    // 脏状态快照 (由心跳 FlushState 周期落盘), 则 LoadState 回填内存快照, 供
    // Lua 在 onInit/onInstall 里通过 GetStateField 读取恢复现场。原型 (id=0) 与
    // 未开启恢复的 Actor 内部直接跳过。Redis 不可用/无快照属正常路径 (首次创建)。
    // ===================================================================
    if (i64ID != 0)
    {
        CRedisCli *stateCli = CGmCommandMgr::instance()->GetCli();
        if (stateCli)
        {
            pActor->LoadState(stateCli); // 无快照/失败仅告警, 不阻断 Install
        }
    }

    // ===================================================================
    // Step 7: 为运行时创建的实例 (id!=0) 挂消息循环。
    //
    // 原型/单例 (id=0) 的消息循环由 ProcessEnv 启动阶段统一 Spawn; 运行时经
    // RPC/createLocal 创建的实例 (BattleActor/PlayerActor 等) 若在此不补挂,
    // 其延迟消息 (如 BattleActor 的 BATTLE_FINALIZE 回收) 会投递进队列却无人
    // 消费, 导致 Actor 永不卸载。
    // ===================================================================
    if (i64ID != 0)
    {
        pActor->BindResumeContext(CActorScheduler::instance()->GetContext());
        CActorScheduler::instance()->Spawn(pActor);
    }

    return true;
}

// UnInstall — 卸载 Actor 运行环境
//
// 与 Install 互为逆操作，执行顺序相反：
//
//   Step 1: Find — 查找 Actor（必须在注册表中）
//   Step 2: actor->UnInstall() — Actor 自身的卸载逻辑
//     典型操作：注销定时器、取消订阅、关闭连接等。
//     即使 UnInstall 失败，仍会继续后续清理步骤（尽力清理原则）。
//
//   Step 3: RemoveInstance — 从注册表移除
//     释放索引条目，但不会 delete Actor 对象（销毁由 ActorCreator 负责）。
//
//   Step 4: tServiceMgr::ReleaseServices — 释放服务引用
//     引用计数 -1；归零时自动销毁服务连接
//
// 注意：UnInstall 不调用 delete——CActor 的生命周期由 ActorCreator 管理。
//       RemoveInstance 仅解除索引关系，Actor 内存由调用方负责释放。
bool CActorMgr::UnInstall(EActorType i32Type, ActorID i64ID)
{
    XLOG("[ActorMgr] UnInstall: BEGIN type=%d id=%lld", i32Type, i64ID);

    // ===================================================================
    // Step 1: Find actor in registry
    // ===================================================================
    auto *pActor = Find(i32Type, i64ID);
    if (!pActor)
    {
        XERR("[ActorMgr] UnInstall: FAIL — actor not found type=%d id=%lld "
             "(already removed or never added)",
             i32Type, i64ID);
        return false;
    }

    XLOG("[ActorMgr] UnInstall: Step 1 OK — found actor name=\"%s\"",
         pActor->GetActorName().c_str());

    // ===================================================================
    // Step 2: Call actor's own UnInstall() virtual method
    // ===================================================================
    //
    // CActor::UnInstall() 是纯虚函数 → CLuaActor::UnInstall() 回调 Lua。
    // Lua 侧通常会执行：
    //   - 注销网络消息回调
    //   - 注销 RPC 处理器
    //   - 停止定时任务
    //   - 清理业务临时状态
    //
    // 记录失败但不中断——后续清理步骤仍会执行，避免资源泄露。
    if (!pActor->UnInstall())
    {
        XERR("[ActorMgr] UnInstall: WARNING — actor->UnInstall() returned false "
             "type=%d id=%lld name=\"%s\" (continuing cleanup anyway)",
             i32Type, i64ID, pActor->GetActorName().c_str());
        // 不 return false——继续执行后续清理
    }
    else
    {
        XLOG("[ActorMgr] UnInstall: Step 2 OK — actor->UnInstall() succeeded");
    }

    // ===================================================================
    // Step 3: Remove from instance registry
    // ===================================================================
    //
    // 调用内部 RemoveInstance（持有锁），从 m_mapInstances 中删除索引条目。
    // 注意：RemoveInstance 返回的 CActor* 在此处不使用——
    //       Actor 对象本身由 ActorCreator::DeleteActor 负责销毁。
    RemoveInstance(i32Type, i64ID);
    XLOG("[ActorMgr] UnInstall: Step 3 OK — removed from instance map");

    // ===================================================================
    // Step 4: Release shared third-party services via tServiceMgr
    // ===================================================================
    //
    // ReleaseServices 对服务引用计数 -1：
    //   - ref_count > 0  → 其他实例仍在使用，不销毁
    //   - ref_count == 0 → 最后一个实例已卸载，自动销毁连接
    //
    // 为什么 Step 2 失败仍要 ReleaseServices？
    //   因为 AcquireServices（Install 时调用）已经增加了引用计数，
    //   无论 UnInstall 自身逻辑是否成功，引用都必须释放。
    tServiceMgr::getMe().ReleaseServices(i32Type);
    XLOG("[ActorMgr] UnInstall: Step 4 OK — released services for type=%d", i32Type);

    // ===================================================================
    // Step 5: Player 实例卸载 → 删除 player:{playerId} → actorId 索引 (M0)
    // ===================================================================
    if (i32Type == EActorType_APlayer && i64ID != 0)
    {
        CPlayerActorIndex::instance()->Deregister((int64)i64ID);
        // PlayerActor 总数 -1 (与 Install 的 IncrPlayerCount 对应)
        CPlayerActorIndex::instance()->DecrPlayerCount();
    }

    // ===================================================================
    // Step 6: 从 Redis 移除 Actor 注册 (与 Install Step 5 对应)
    // ===================================================================
    CGmCommandMgr::instance()->DeregisterActor(i32Type, i64ID);

    // ===================================================================
    // Step 7: 让恢复快照 actor:state:{type}:{id} 按配置失效时长过期 (实例 id!=0)
    //
    // 干净卸载不再立即 Del, 而是按 Actor 类型配置的 TTL 过期 (见 Actor::DeleteState):
    // 删除后短暂可恢复, 到期由 Redis 过期清理。进程宕机(未走此路径)时快照同样由 TTL 兜底。
    // ===================================================================
    if (i64ID != 0)
    {
        CRedisCli *stateCli = CGmCommandMgr::instance()->GetCli();
        if (stateCli)
        {
            pActor->DeleteState(stateCli);
        }
    }

    XLOG("[ActorMgr] UnInstall: OK — type=%d id=%lld all steps complete", i32Type, i64ID);
    return true;
}

// InstallAll — 批量安装指定类型的所有已注册实例
//
// 遍历 m_mapInstances[type] 中所有 ID，逐个调用 Install。
// 典型场景：
//   - 服务器启动后，从 DB 加载所有持久化的 Player 实例并 Install
//   - 热更后重新 Install 所有已存在的 Actor
//
// 注意：先收集指针向量再释放锁遍历（Install 内部会再次加锁 Find，
//       但 Find 只做查找，不会死锁——std::mutex 不可重入，但不同的
//       调用路径使用独立的锁作用域，这里收集后释放了锁）。
//
// 返回值：成功 Install 的数量（≤ 该类型的总实例数）
int32 CActorMgr::InstallAll(EActorType i32Type)
{
    XLOG("[ActorMgr] InstallAll: BEGIN type=%d", i32Type);

    // --- 在锁内收集所有实例指针 ---
    // 为什么要先收集再遍历？
    //   1. Install 内部会调用 Find，如果直接在锁内遍历会导致嵌套锁尝试
    //   2. Install 可能耗时较长（创建服务连接），不宜长时间持有锁
    std::vector<CActor *> actors;
    {
        std::lock_guard<std::mutex> lock(m_stLock);
        auto outerIt = m_mapInstances.find(i32Type);
        if (outerIt == m_mapInstances.end())
        {
            XLOG("[ActorMgr] InstallAll: type=%d has no instances, nothing to do", i32Type);
            return 0;
        }
        for (const auto &pair : outerIt->second)
        {
            actors.push_back(pair.second);
        }
        XLOG("[ActorMgr] InstallAll: collected %zu actor(s) for type=%d", actors.size(), i32Type);
    }
    // 锁在此处释放

    // --- 逐个 Install ---
    int32 count = 0;
    for (auto *pActor : actors)
    {
        EActorType type = pActor->GetActorType(); // = i32Type（same for all）
        ActorID id = pActor->GetActorID();

        XLOG("[ActorMgr] InstallAll: [%d/%zu] installing type=%d id=%lld name=\"%s\"", count + 1,
             actors.size(), type, id, pActor->GetActorName().c_str());

        if (Install(type, id))
        {
            count++;
        }
        else
        {
            XERR("[ActorMgr] InstallAll: [%d/%zu] SKIP — Install failed type=%d id=%lld", count + 1,
                 actors.size(), type, id);
        }
    }

    XLOG("[ActorMgr] InstallAll: DONE type=%d — %d/%zu succeeded, %zu failed", i32Type, count,
         actors.size(), actors.size() - count);
    return count;
}

// UnInstallAll — 批量卸载指定类型的所有实例
//
// 与 InstallAll 对应：收集所有实例 → 逐个 UnInstall。
// 典型场景：
//   - 服务器关闭前卸载所有 Actor
//   - 进程级别的重载操作
//
// 返回值：成功 UnInstall 的数量
int32 CActorMgr::UnInstallAll(EActorType i32Type)
{
    XLOG("[ActorMgr] UnInstallAll: BEGIN type=%d", i32Type);

    // --- 在锁内收集所有实例指针 ---
    std::vector<CActor *> actors;
    {
        std::lock_guard<std::mutex> lock(m_stLock);
        auto outerIt = m_mapInstances.find(i32Type);
        if (outerIt == m_mapInstances.end())
        {
            XLOG("[ActorMgr] UnInstallAll: type=%d has no instances, nothing to do", i32Type);
            return 0;
        }
        for (const auto &pair : outerIt->second)
        {
            actors.push_back(pair.second);
        }
        XLOG("[ActorMgr] UnInstallAll: collected %zu actor(s) for type=%d", actors.size(), i32Type);
    }
    // 锁在此处释放

    // --- 逐个 UnInstall ---
    int32 count = 0;
    for (auto *pActor : actors)
    {
        EActorType type = pActor->GetActorType();
        ActorID id = pActor->GetActorID();

        XLOG("[ActorMgr] UnInstallAll: [%d/%zu] uninstalling type=%d id=%lld name=\"%s\"",
             count + 1, actors.size(), type, id, pActor->GetActorName().c_str());

        if (UnInstall(type, id))
        {
            count++;
        }
        else
        {
            XERR("[ActorMgr] UnInstallAll: [%d/%zu] SKIP — UnInstall failed type=%d id=%lld",
                 count + 1, actors.size(), type, id);
        }
    }

    XLOG("[ActorMgr] UnInstallAll: DONE type=%d — %d/%zu succeeded, %zu failed", i32Type, count,
         actors.size(), actors.size() - count);
    return count;
}

// ============================================================================
// Query Operations（查询操作）
// ============================================================================

// Find — 按 (type, id) 精确查找 Actor
//
// 时间复杂度：O(1) 外层 + O(log n) 内层（map::find）
// 线程安全：持有读锁
//
// 返回值：
//   - 找到：CActor* 裸指针（生命周期由 m_mapInstances 管理，调用方不持有）
//   - 未找到：nullptr
//
// 注意：返回的指针在外层 UnInstall/RemoveInstance 后即失效，
//       调用方不应长期持有该指针。
CActor *CActorMgr::Find(EActorType i32Type, ActorID i64ID)
{
    std::lock_guard<std::mutex> lock(m_stLock);

    // --- 按类型查找外层 ---
    auto outerIt = m_mapInstances.find(i32Type);
    if (outerIt == m_mapInstances.end())
    {
        XDBG("[ActorMgr] Find: miss — type=%d not in registry", i32Type);
        return nullptr;
    }

    // --- 按 ID 查找内层 ---
    auto &innerMap = outerIt->second;
    auto innerIt = innerMap.find(i64ID);
    if (innerIt == innerMap.end())
    {
        XDBG("[ActorMgr] Find: miss — type=%d found (%zu entries), but id=%lld not among them",
             i32Type, innerMap.size(), i64ID);
        return nullptr;
    }

    XDBG("[ActorMgr] Find: hit — type=%d id=%lld name=\"%s\"", i32Type, i64ID,
         innerIt->second->GetActorName().c_str());
    return innerIt->second;
}

// FindByActorID — 仅凭 ActorID 在所有托管类型中查找实例
//
// 用于网络路由：NetPackageHeadST 只携带 u64TargetActorId (扁平 ActorID)，
// 不含类型，因此需要遍历各类型按 ID 匹配。本进程通常只托管单一类型，
// 多数情况下即 O(1) 命中外层唯一类型。
CActor *CActorMgr::FindByActorID(ActorID i64ID)
{
    std::lock_guard<std::mutex> lock(m_stLock);

    for (auto &outerPair : m_mapInstances)
    {
        auto innerIt = outerPair.second.find(i64ID);
        if (innerIt != outerPair.second.end())
        {
            XDBG("[ActorMgr] FindByActorID: hit type=%d id=%lld name=\"%s\"", outerPair.first,
                 i64ID, innerIt->second->GetActorName().c_str());
            return innerIt->second;
        }
    }

    XDBG("[ActorMgr] FindByActorID: miss id=%lld", i64ID);
    return nullptr;
}

// GetByType — 获取指定类型的所有 Actor 实例
//
// 返回值：CActor* 的 vector 拷贝（调用方持有副本，不持有 Actor 所有权）
// 如果没有实例，返回空 vector（不是 nullptr）。
//
// 典型场景：
//   - 统计面板：列出某类型所有实例的状态
//   - 广播通知：对某类型的所有实例发送消息
std::vector<CActor *> CActorMgr::GetByType(EActorType i32Type)
{
    std::lock_guard<std::mutex> lock(m_stLock);

    std::vector<CActor *> result;

    auto outerIt = m_mapInstances.find(i32Type);
    if (outerIt == m_mapInstances.end())
    {
        XDBG("[ActorMgr] GetByType: type=%d — no instances", i32Type);
        return result; // 返回空 vector
    }

    // --- 复制所有指针到结果 ---
    for (const auto &pair : outerIt->second)
    {
        result.push_back(pair.second);
    }

    XDBG("[ActorMgr] GetByType: type=%d — %zu instance(s) returned", i32Type, result.size());
    return result;
}

// GetInstanceCount — 获取本进程管理的 Actor 总实例数
//
// 时间复杂度：O(K)，K = 不同 EActorType 的数量
// 线程安全：持有读锁
//
// 典型用途：
//   - 负载均衡器获取当前进程负载指标
//   - 监控/告警：实例数异常增长检测
size_t CActorMgr::GetInstanceCount() const
{
    std::lock_guard<std::mutex> lock(m_stLock);

    size_t count = 0;
    for (const auto &outerPair : m_mapInstances)
    {
        count += outerPair.second.size();
    }

    XDBG("[ActorMgr] GetInstanceCount: total=%zu across %zu type(s)", count, m_mapInstances.size());
    return count;
}
