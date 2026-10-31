/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: Actor.h
      Purpose:   CActor 基类声明

      定义 Actor 通用属性(类型/ID/名称/负载分值/状态/归属/全局ID)与 ActorReport 上报结构
      声明帧级脏读写 Redis 状态框架(SetStateField/GetStateField/FlushState/LoadState)
      声明消息队列与协程 MessageLoop, OnMessage/HandleSystemMessage 虚钩子
  *****************************************************************/

#pragma once

#include "ActorDefine.h"
#include "ActorMessage.h"

#include <cstdio>
#include <ctime>
#include <map>
#include <mutex>
#include <string>

class CRedisCli;

class CActor
{
public:
    CActor(EActorType i32ActorType, ActorID i64ActorID, const std::string &strActorName)
        : m_i32ActorType(i32ActorType), m_i64ActorID(i64ActorID), m_strActorName(strActorName)
    {
        // 原型 (id=0) 不参与状态持久化/恢复, 只有实例 (id!=0) 开启
        m_bRecoveryEnabled = (i64ActorID != 0);
        // 创建时间戳: 构造即记录, 供展示/上报结构读取 (unix seconds)
        m_i64CreateTime = (int64)time(nullptr);
    }
    virtual ~CActor()
    {
    }

public:
    EActorType GetActorType()
    {
        return m_i32ActorType;
    }
    ActorID GetActorID()
    {
        return m_i64ActorID;
    }
    const std::string &GetActorName()
    {
        return m_strActorName;
    }
    // 全局唯一 ID (xActorId::MakeActorId 铸币, 展示/上报用; 与业务键 m_i64ActorID 平行)
    uint64 GetGlobalId() const
    {
        return m_i64GlobalId;
    }
    void SetGlobalId(uint64 v)
    {
        m_i64GlobalId = v;
    }

public:
    void SetCreateEnable(bool bEnable)
    {
        m_bCreateEnable = bEnable;
    }
    bool CreateIsEnable()
    {
        return m_bCreateEnable;
    }

    // --- Actor 通用数据 Getter/Setter (C++/Lua 共用) ---

    int32 GetLoadScore() const
    {
        return m_i32LoadScore;
    }
    void SetLoadScore(int32 v)
    {
        m_i32LoadScore = v;
    }

    int64 GetCreateTime() const
    {
        return m_i64CreateTime;
    }
    void SetCreateTime(int64 v)
    {
        m_i64CreateTime = v;
    }

    int32 GetState() const
    {
        return m_i32State;
    }
    void SetState(int32 v)
    {
        m_i32State = v;
    }

    ActorID GetOwnerID() const
    {
        return m_i64OwnerID;
    }
    void SetOwnerID(ActorID v)
    {
        m_i64OwnerID = v;
    }

    // --- Actor 上报结构 (监控上报模块用) ---
    // 增减上报字段 = 改此结构 + BuildReportJson 序列化 + actor_info.php 展示, 三处同步即可。
    struct ActorReport
    {
        EActorType type = EActorType_None; // 业务类型
        ActorID id = 0;                    // 业务键 (roleId/battleId; 单例/原型=0)
        uint64 globalId = 0;               // 全局唯一 ID (xActorId::MakeActorId)
        std::string name;
        int32 loadScore = 0;
        int64 createTime = 0;
        int32 state = 0;
        ActorID ownerId = 0;
        int64 reportedAt = 0; // 本次上报时间戳
    };

    // 序列化上报结构为单行 JSON (本仓库无 JSON 库, 手拼; 实现见 Actor.cpp)
    std::string BuildReportJson(int64 nowSec) const;

    // --- Actor 监控上报扩展 (业务在调度线程写, 心跳线程 Sweep 读) ---
    // 业务脚本 (如 BattleActor) 经 si_actor_set_report_extra 写入本场核心状态
    // (BattleGame / HeroState) 的 JSON 片段; BuildReportJson 把它作为 "battle" 子对象拼入。
    // 读写经 m_reportMutex 串行化, 绝不触碰进程唯一 lua_State。
    void SetReportExtraJson(const std::string &json)
    {
        std::lock_guard<std::mutex> lock(m_reportMutex);
        m_reportExtra = json;
    }
    std::string GetReportExtraJson() const
    {
        std::lock_guard<std::mutex> lock(m_reportMutex);
        return m_reportExtra;
    }

    // --- Actor 帧级脏读写 Redis 状态框架 (P7) ---
    // 业务通过 SetStateField / GetStateField 做「脏读写」:
    //   - 写入只更新内存快照并标记脏字段, 不立即落 Redis (write-behind)。
    //   - 读取优先本地快照 (含未落盘的脏数据), 缺字段再回源 Redis。
    // 框架按周期 (心跳) 调用 FlushState 把脏字段批量写 actor:state:{type}:{id},
    // 供 Actor 下线/进程宕机后由 LoadState 动态恢复。恢复/落盘事件写 actor:recovery:log。

    void SetStateField(const std::string &key, const std::string &value);
    bool GetStateField(const std::string &key, std::string &out) const;

    void SetRecoveryEnabled(bool b)
    {
        m_bRecoveryEnabled = b;
    }
    bool IsRecoveryEnabled() const
    {
        return m_bRecoveryEnabled;
    }
    bool HasDirtyState() const; // 是否有待落盘脏字段

    // 落盘: 把脏字段写 actor:state:{type}:{id} (无脏数据直接返回 true, 不写日志)
    bool FlushState(CRedisCli *cli);
    // 恢复: 读 actor:state:{type}:{id} 回填内存快照 (返回 false = 无快照/失败)
    bool LoadState(CRedisCli *cli);

    // 心跳续期: 刷新 actor:state:{type}:{id} 的 TTL (进程宕机后由 Redis 过期兜底清理)。
    // TTL 按类型从 cluster:config:actor-state-ttl 读取, 缺省 86400。id=0 原型/未开启恢复直接返回 true。
    bool RefreshStateTtl(CRedisCli *cli);
    // 干净卸载时按类型配置的失效时长过期 (而非立即 Del): 删除后短暂可恢复, 到期 Redis 过期清理
    bool DeleteState(CRedisCli *cli);

    // Redis 状态 Hash key: actor:state:{type}:{id}
    std::string StateRedisKey() const;

    // 写一条恢复日志到 actor:recovery:log (格式 {ts}|{type}|{id}|{event}|{detail})
    void LogRecovery(CRedisCli *cli, const char *event, const std::string &detail);

public:
    virtual bool Init() = 0;

public:
    // 进程负载 环境初始化
    virtual bool Install() = 0;
    virtual bool UnInstall() = 0;

public:
    virtual CActor *Create(ActorID i64ActorID) = 0;

public:
    // --- 消息队列与协程消息循环 ---
    // 向本 Actor 投递一条消息 (线程安全，任意线程可调用)
    void PostMessage(uint32 u32MsgId, const std::string &strBody);

    // 投递空 body 停止信号：消息循环消费后 break, 协程帧自毁 (final_suspend=suspend_never)
    void RequestStop();

    // 绑定协程恢复执行器 (调度器的 io_context)
    void BindResumeContext(asio::io_context &ioc);

    // 协程消息循环：消费队列 → OnMessage；空 body 为停止信号
    ActorTask MessageLoop();

    // 消息循环是否已挂载 (防重复 Spawn 造成单队列双消费者)
    bool IsMessageLoopSpawned() const { return m_bMessageLoopSpawned; }
    void MarkMessageLoopSpawned() { m_bMessageLoopSpawned = true; }

    // 收到一条消息 (默认空实现；CLuaActor 覆写转发到 Lua on_msg)
    virtual void OnMessage(uint32 u32MsgId, const std::string &strBody)
    {
        (void)u32MsgId;
        (void)strBody;
    }

protected:
    // 框架系统消息 (高 16 位 == 0) 的处理钩子。默认不处理返回 false。
    // 业务消息 (高 16 位 != 0) 不走这里, 直接进 OnMessage → Lua on_msg。
    virtual bool HandleSystemMessage(uint32 u32MsgId, const std::string &strBody)
    {
        (void)u32MsgId;
        (void)strBody;
        return false;
    }

private:
    // 按 Actor 类型解析状态快照失效时长 (读 cluster:config:actor-state-ttl, 缺省 86400)
    int64 ResolveStateTtlSeconds(CRedisCli *cli) const;

    EActorType m_i32ActorType = EActorType_None;
    ActorID m_i64ActorID = 0;
    std::string m_strActorName;

    // Actor 通用数据 — C++/Lua 共享的 actor 基础属性
    int32 m_i32LoadScore = 0;  // 负载分值
    int64 m_i64CreateTime = 0; // 创建时间戳 (unix seconds)
    int32 m_i32State = 0;      // actor 状态: 0=init, 1=active, 2=paused, 3=removed
    ActorID m_i64OwnerID = 0;  // 所属者 ActorID (0=无归属)
    uint64 m_i64GlobalId = 0;  // 全局唯一 ID (xActorId::MakeActorId 铸币)

private:
    // 是否允许创建
    bool m_bCreateEnable = true;

    // 帧级脏读写状态框架: 内存快照 + 待落盘脏字段
    bool m_bRecoveryEnabled = true;    // id!=0 实例默认开启持久化/恢复
    mutable std::mutex m_stStateMutex; // 保护状态 map (写线程/心跳 flush 并发)
    std::map<std::string, std::string> m_mapState; // 内存态快照 (脏读缓存, 含未落盘脏数据)
    std::map<std::string, std::string> m_mapDirty; // 待落盘脏字段 (FlushState 消费)

    // 监控上报扩展: 业务在调度线程写 (SetReportExtraJson), 心跳线程读 (GetReportExtraJson)
    mutable std::mutex m_reportMutex;
    std::string m_reportExtra;

private:
    // 消息循环是否已挂载 (Spawn 幂等守卫)
    bool m_bMessageLoopSpawned = false;

    // 消息队列 (供 MessageLoop 协程消费)
    ActorMessageQueue m_stMessageQueue;
};

typedef std::map<EActorType, CActor *> CActorTypeMap;
typedef std::map<ActorID, CActor *> CActorMap;
