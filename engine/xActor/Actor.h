#pragma once


#include "ActorDefine.h"


class CActor
{
public:
    CActor(ActorType i32ActorType, ActorID i64ActorID, const std::string &strActorName)
        : m_i32ActorType(i32ActorType), m_i64ActorID(i64ActorID), m_strActorName(strActorName) {
    }
    ~CActor() {
    }

public:
    ActorType GetActorType() { return m_i32ActorType; }
    ActorID GetActorID() { return m_i64ActorID; }
    const std::string& GetActorName() { return m_strActorName; }

public:
    void SetCreateEnable(bool bEnable) { m_bCreateEnable = bEnable; }
    bool CreateIsEnable() { return m_bCreateEnable; }

public:
    virtual bool Init() = 0;

public:
    // 进程负载 环境初始化
    virtual bool Install() = 0;
    virtual bool UnInstall() = 0;

public:
    virtual CActor* Create(ActorID i64ActorID) = 0;

private:
    ActorType   m_i32ActorType  = 0;
    ActorID     m_i64ActorID    = 0;
    std::string m_strActorName;

private:
    // 是否允许创建
    bool m_bCreateEnable = true;
};

typedef std::map<ActorType, CActor*> CActorTypeMap;
typedef std::map<ActorID, CActor*> CActorMap;

//
//class CActorPlayer : public CActor
//{
//public:
//    CActorPlayer(ActorID i64ActorID) : CActor(ACTOR_TYPE_PLAYER, i64ActorID, "CActorPlayer") {}
//    ~CActorPlayer(){}
//
//private:
//    // 负责 从mysql redis 加载数据
//    virtual bool Init() {
//
//        return true;
//    }
//
//public:
//    // 单例创建接口
//    CActor* Create(ActorID i64ActorID) {
//        auto pActor = new CActorPlayer(i64ActorID);
//        if (!pActor) {
//            return nullptr;
//        }
//        // can not create
//        if (!pActor->CreateIsEnable()) {
//            delete pActor;
//            return nullptr;
//        }
//        if (!pActor->Init()) {
//            delete pActor;
//            return nullptr;
//        }
//        return pActor;
//    }
//};
//
//class CActorScene : public CActor
//{
//public:
//    CActorScene(ActorID i64ActorID) : CActor(ACTOR_TYPE_PLAYER, i64ActorID, "CActorScene") {}
//    ~CActorScene() {}
//
//private:
//    // 负责 从mysql redis 加载数据
//    virtual bool Init() {
//
//        return true;
//    }
//
//public:
//    // 单例创建接口
//    CActor* Create(ActorID i64ActorID) {
//        auto pActor = new CActorScene(i64ActorID);
//        if (!pActor) {
//            return nullptr;
//        }
//        // can not create
//        if (!pActor->CreateIsEnable()) {
//            delete pActor;
//            return nullptr;
//        }
//        if (!pActor->Init()) {
//            delete pActor;
//            return nullptr;
//        }
//        return pActor;
//    }
//};


/*
void test() {

    //auto actor = ActorCreator::getMe().Create(ACTOR_TYPE_PLAYER, 100);

    

}
*/

