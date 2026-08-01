#pragma once

#include "Actor.h"
#include "ActorMgr.h"
#include "xBase/xSingleton.h"
#include "xLog/xLog.h"


class ActorCreator : xSingleton<ActorCreator>
{
public:
    ActorCreator() {}
    ~ActorCreator() {}

public:
    // --- 原型注册 ---
    static bool RegistActor(CActor* actor) {
        if (!actor) {
            return false;
        }
        auto it = m_mapActorType.find(actor->GetActorType());
        if (it != m_mapActorType.end()) {
            return false;
        }
        m_mapActorType[actor->GetActorType()] = actor;
        XLOG("[INIT] ActorCreator registered prototype: type=%d name=%s",
             actor->GetActorType(), actor->GetActorName().c_str());
        return true;
    }

    static void SetCreateEnable(ActorType enActorType, bool bEnable) {
        auto it = m_mapActorType.find(enActorType);
        if (it == m_mapActorType.end()) {
            return;
        }
        auto* pActor = it->second;
        if (!pActor) {
            return;
        }
        pActor->SetCreateEnable(bEnable);
    }

public:
    // --- 本地实例创建 (从原型 clone) ---
    static CActor* Create(ActorType enActorType, ActorID i64ActorID) {
        auto it = m_mapActorType.find(enActorType);
        if (it == m_mapActorType.end()) {
            return nullptr;
        }
        auto* pActor = it->second;
        if (!pActor) {
            return nullptr;
        }
        if (!pActor->CreateIsEnable()) {
            XWRN("[INIT] ActorCreator::Create disabled for type=%d", enActorType);
            return nullptr;
        }
        if (!pActor->Init()) {
            XERR("[INIT] ActorCreator::Create Init failed for type=%d", enActorType);
            return nullptr;
        }
        auto* result = pActor->Create(i64ActorID);
        if (result) {
            XLOG("[INIT] ActorCreator created: type=%d id=%lld", enActorType, i64ActorID);
        }
        return result;
    }

public:
    // --- 全局 RPC 接口 (供负载均衡器/跨进程调用) ---

    // 获取本进程 actor 实例总数 (作为负载分值基础)
    static int32 GetLocalInstanceCount() {
        return (int32)CActorMgr::instance()->GetInstanceCount();
    }

    // 获取某 type 在本进程的实例数
    static int32 GetLocalTypeCount(ActorType enActorType) {
        auto actors = CActorMgr::instance()->GetByType(enActorType);
        return (int32)actors.size();
    }

    // 动态删除本进程的一个 actor 实例
    // (先 UnInstall 再从 CActorMgr 移除, 不 delete 内存 — 由 Lua GC 管理)
    static bool DeleteActor(ActorType enActorType, ActorID i64ActorID) {
        return CActorMgr::instance()->UnInstall(enActorType, i64ActorID);
    }

private:
    static CActorTypeMap m_mapActorType;
};
