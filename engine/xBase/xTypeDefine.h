/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: xTypeDefine.h
      Purpose:   全局类型系统枚举定义

      定义 EProcessType（进程类型）、EActorType（Actor 类型）与 EProcessState（进程状态）枚举
      消除裸整数别名，供 xActor/tService/xConfig 等基础层引用
  *****************************************************************/

#pragma once

// =============================================================================
// xTypeDefine.h — 全局类型系统枚举 (进程类型 / Actor 类型 / 进程状态)
//
// 放到 xBase 而不是 xCluster, 是为了让 xActor / tService / xConfig 等基础层
// 也能直接引用这些枚举, 从而消除原先 `typedef int32 ActorType;` 这类裸整数
// 别名 (严格区分进程枚举 EProcessType 与 Actor 枚举 EActorType)。
//
// 依赖: 枚举基类型使用 `int32` (xBase/xDefine.h 的自定义整数别名), 因此本头
// 文件必须由 xBase/xDefine.h 在 int32 定义之后再 include (xDefine.h 已如此安排)。
// =============================================================================

// 进程类型 (P 前缀): 部署容器类型, 决定本进程可承载哪些 Actor 类型
enum EProcessType : int32
{
    EProcessType_None = 0,
    EProcessType_PGate = 100,   // 网关进程
    EProcessType_PLogin = 101,  // 登录进程
    EProcessType_PGlobal = 102, // 全局进程 (承载 Friend/Chat/Config Actor)
    EProcessType_PGame = 201,   // 游戏进程 (承载 Player/Room Actor)
    EProcessType_PBattle = 202, // 战斗进程 (承载 Battle/Spectator Actor, 客户端直连)
    EProcessType_Max
};

// Actor 类型 (A 前缀): 进程内动态负载的 Actor 类, 与进程类型解耦
enum EActorType : int32
{
    EActorType_None = 0,
    EActorType_AGate = 100,      // 网关 Actor
    EActorType_ALogin = 101,     // 登录 Actor
    EActorType_AFriend = 102,    // 好友 Actor (单例, Global 进程)
    EActorType_ARoom = 103,      // 房间 Actor (单例, Game 进程)
    EActorType_AChat = 104,      // 聊天 Actor (单例, Global 进程)
    EActorType_AConfig = 105,    // 配置 Actor (单例, Global 进程, 配置热更拉取/推送)
    EActorType_APlayer = 201,    // 玩家 Actor (Game 进程)
    EActorType_ABattle = 203,    // 战斗 Actor (多实例, Battle 进程)
    EActorType_ASpectator = 204, // 观战 Actor (多实例, Battle 进程)
    EActorType_Max
};

// 进程运行状态
enum EProcessState : int32
{
    EProcessState_Starting = 0, // 启动中
    EProcessState_Init = 1,     // 初始化完成
    EProcessState_Running = 2,  // 运行中
    EProcessState_Paused = 3,   // 暂停
    EProcessState_Stopping = 4, // 停用中
    EProcessState_Shutdown = 5, // 已关闭
};
