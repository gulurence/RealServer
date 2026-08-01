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

    // --- Actor 通用数据 Getter/Setter (C++/Lua 共用) ---

    int32  GetLoadScore() const   { return m_i32LoadScore; }
    void   SetLoadScore(int32 v)  { m_i32LoadScore = v; }

    int64  GetCreateTime() const  { return m_i64CreateTime; }
    void   SetCreateTime(int64 v) { m_i64CreateTime = v; }

    int32  GetState() const       { return m_i32State; }
    void   SetState(int32 v)      { m_i32State = v; }

    ActorID GetOwnerID() const    { return m_i64OwnerID; }
    void    SetOwnerID(ActorID v) { m_i64OwnerID = v; }

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

    // Actor 通用数据 — C++/Lua 共享的 actor 基础属性
    int32  m_i32LoadScore  = 0;       // 负载分值
    int64  m_i64CreateTime = 0;       // 创建时间戳 (unix seconds)
    int32  m_i32State      = 0;       // actor 状态: 0=init, 1=active, 2=paused, 3=removed
    ActorID m_i64OwnerID   = 0;       // 所属者 ActorID (0=无归属)

private:
    // 是否允许创建
    bool m_bCreateEnable = true;
};

typedef std::map<ActorType, CActor*> CActorTypeMap;
typedef std::map<ActorID, CActor*> CActorMap;

