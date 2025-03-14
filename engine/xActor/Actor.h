﻿#pragma once


#include "ActorDefine.h"


class CActor
{
public:
    CActor(ActorType i32ActorType, ActorID i64ActorID, const std::string &strActorName):m_i32ActorType(i32ActorType), m_i64ActorID(i64ActorID), m_strActorName(strActorName){
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
    virtual int32 OnRegist() = 0;
    virtual int32 OnUnRegist() = 0;
    virtual CActor* Create(ActorID i64ActorID) = 0;

private:
    ActorType   m_i32ActorType  = 0;
    ActorID     m_i64ActorID    = 0;
    std::string m_strActorName;

private:
    // 是否允许创建
    bool        m_bCreateEnable = true;
};
typedef std::map<ActorType, CActor*> CActorTypeMap;
typedef std::map<ActorID, CActor*> CActorMap;


class Player : public CActor
{
public:
    Player(ActorType enActorType, ActorID i64ActorID, const std::string& strActorName) :CActor(enActorType, i64ActorID, strActorName) {}
    ~Player(){}

private:
    // 负责 从mysql redis 加载数据
    virtual bool Init() {


        return true;
    }

public:
    // 单例创建接口
    CActor* Create(ActorType enActorType, ActorID i64ActorID, const std::string& strActorName) {
        auto pActor = new Player(enActorType, i64ActorID, strActorName);
        if (!pActor) {
            return nullptr;
        }
        // can not create
        if (!pActor->CreateIsEnable()) {
            delete pActor;
            return nullptr;
        }
        if (!pActor->Init()) {
            delete pActor;
            return nullptr;
        }
        return pActor;
    }
    // 负责进行Actor的信息注册
    virtual int32 Install() {

        return 0;
    }
    // 负责进行Actor的信息注销
    virtual int32 UnInstall() {

        return 0;
    }
};


class ActorCreator
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
    static void SetCreateEnable(ActorType enActorType, bool bEnable){
        auto it = m_mapActorType.find(enActorType);
        if (it != m_mapActorType.end()) {
            return ;
        }
        auto* pActor = it->second;
        if (!pActor) {
            return ;
        }
        pActor->SetCreateEnable(bEnable);
    }

public:
    static CActor *Create(ActorType enActorType, ActorID i64ActorID, const std::string& strActorName) {
        auto it = m_mapActorType.find(enActorType);
        if (it == m_mapActorType.end()) {
            return nullptr;
        }
        auto* pActor = it->second;
        if (!pActor) {
            return nullptr;
        }
        return pActor->Create(enActorType, i64ActorID, strActorName);
    }

private:
    static CActorTypeMap m_mapActorType;
};

