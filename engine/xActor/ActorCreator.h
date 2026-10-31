/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: ActorCreator.h
      Purpose:   Actor 原型注册与工厂创建

      ActorCreator 单例: RegistActor 注册原型, SetCreateEnable 控制类型创建开关
      Create 从原型克隆实例, DeleteActor 卸载并删除实例
      提供 GetLocalInstanceCount/GetLocalTypeCount 负载统计接口
  *****************************************************************/

#pragma once

#include "Actor.h"
#include "ActorMgr.h"
#include "xBase/xSingleton.h"
#include "xLog/xLog.h"

class ActorCreator : xSingleton<ActorCreator>
{
public:
    ActorCreator()
    {
    }
    ~ActorCreator()
    {
    }

public:
    // --- 原型注册 ---
    static bool RegistActor(CActor *actor)
    {
        if (!actor)
        {
            return false;
        }
        auto it = m_mapActorType.find(actor->GetActorType());
        if (it != m_mapActorType.end())
        {
            return false;
        }
        m_mapActorType[actor->GetActorType()] = actor;
        XLOG("[INIT] ActorCreator registered prototype: type=%d name=%s", actor->GetActorType(),
             actor->GetActorName().c_str());
        return true;
    }

    static void SetCreateEnable(EActorType enActorType, bool bEnable)
    {
        auto it = m_mapActorType.find(enActorType);
        if (it == m_mapActorType.end())
        {
            return;
        }
        auto *pActor = it->second;
        if (!pActor)
        {
            return;
        }
        pActor->SetCreateEnable(bEnable);
    }

public:
    // --- 本地实例创建 (从原型 clone) ---
    static CActor *Create(EActorType enActorType, ActorID i64ActorID)
    {
        auto it = m_mapActorType.find(enActorType);
        if (it == m_mapActorType.end())
        {
            return nullptr;
        }
        auto *pActor = it->second;
        if (!pActor)
        {
            return nullptr;
        }
        if (!pActor->CreateIsEnable())
        {
            XWRN("[INIT] ActorCreator::Create disabled for type=%d", enActorType);
            return nullptr;
        }
        if (!pActor->Init())
        {
            XERR("[INIT] ActorCreator::Create Init failed for type=%d", enActorType);
            return nullptr;
        }
        auto *result = pActor->Create(i64ActorID);
        if (result)
        {
            XLOG("[INIT] ActorCreator created: type=%d id=%lld", enActorType, i64ActorID);
        }
        return result;
    }

public:
    // --- 全局 RPC 接口 (供负载均衡器/跨进程调用) ---

    // 获取本进程 actor 实例总数 (作为负载分值基础)
    static int32 GetLocalInstanceCount()
    {
        return (int32)CActorMgr::instance()->GetInstanceCount();
    }

    // 获取某 type 在本进程的实例数
    static int32 GetLocalTypeCount(EActorType enActorType)
    {
        auto actors = CActorMgr::instance()->GetByType(enActorType);
        return (int32)actors.size();
    }

    // 动态删除本进程的一个 actor 实例
    // (Find → UnInstall 移除注册表/Redis/恢复快照 → delete 释放 C++ 内存)。
    // 原型 (id=0) 进程生命周期内不删除, 保留给后续 Create 克隆。
    static bool DeleteActor(EActorType enActorType, ActorID i64ActorID)
    {
        CActor *pActor = CActorMgr::instance()->Find(enActorType, i64ActorID);
        if (!pActor)
        {
            return false;
        }
        bool ok = CActorMgr::instance()->UnInstall(enActorType, i64ActorID);
        if (i64ActorID != 0)
        {
            delete pActor;
        }
        return ok;
    }

private:
    static CActorTypeMap m_mapActorType;
};
