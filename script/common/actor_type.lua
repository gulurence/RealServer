--[[
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: actor_type.lua
      Purpose:   Actor 类型枚举定义

      定义 xEngine.ActorType，与 C++ EActorType 对齐：Gate=100、Login=101、Friend=102、Room=103、Chat=104、Config=105、Player=201、Battle=203、Spectator=204。
]]

-- actor_type.lua — Actor type enum definitions
-- Shared between C++ (EActorType enum) and Lua layer
-- Must match the values in engine/xBase/xTypeDefine.h EActorType

xEngine = xEngine or {}

xEngine.ActorType = {
    -- Gate 网关 (100)
    GATE  = 100,

    -- Login 登录 (101)
    LOGIN = 101,

    -- Friend 好友 (102, 单例, Global 进程)
    FRIEND = 102,

    -- Room 房间 (103, 单例, Game 进程)
    ROOM = 103,

    -- Chat 聊天 (104, 单例, Global 进程)
    CHAT = 104,

    -- Config 配置 (105, 单例, Global 进程, 配置热更拉取/推送)
    CONFIG = 105,

    -- Player 玩家 (201, Game 进程)
    PLAYER = 201,

    -- Battle 战斗 (203, 多实例, Battle 进程)
    BATTLE = 203,

    -- Spectator 观战 (204, 多实例, Battle 进程)
    SPECTATOR = 204,
}
