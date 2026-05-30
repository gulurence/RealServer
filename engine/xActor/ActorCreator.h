#pragma once

#include "Actor.h"
#include "xBase/xSingleton.h"


class ActorCreator : xSingleton<ActorCreator>
{
public:
    ActorCreator() {}
    ~ActorCreator() {}

private:
    static bool RegistActor(CActor* actor) {
        if (!actor) {
            return false;
        }
        auto it = m_mapActorType.find(actor->GetActorType());
        if (it != m_mapActorType.end()) {
            return false;
        }
        m_mapActorType[actor->GetActorType()] = actor;
    }

    static void SetCreateEnable(ActorType enActorType, bool bEnable) {
        auto it = m_mapActorType.find(enActorType);
        if (it != m_mapActorType.end()) {
            return;
        }
        auto* pActor = it->second;
        if (!pActor) {
            return;
        }
        pActor->SetCreateEnable(bEnable);
    }

public:
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
            return nullptr;
        }
        if (!pActor->Init()) {
            return nullptr;
        }
        return pActor->Create(i64ActorID);
    }

private:
    static CActorTypeMap m_mapActorType;
};
