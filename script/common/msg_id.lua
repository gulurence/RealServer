--[[
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: msg_id.lua
      Purpose:   消息 ID 约定定义

      xEngine.MsgId 定义 32 位消息 ID 分段（高 16 位类别、低 16 位子编号）及 USER_BEGIN 业务消息。
      NAMES 表登记 msg_id 到 handler 名后缀（on_<name>_msg）的映射，供 ActorBase:on_msg 分发。
]]

-- msg_id.lua — 消息 ID 约定 (Lua 侧)
-- 镜像 C++ engine/xActor/ActorMsgId.h 的 32 位 ID 分段约定:
--   高 16 位 = 类别 (0 = 框架系统消息/C++ 处理; 非0 = 业务消息/Lua 处理)
--   低 16 位 = 类别内子编号
-- 网络包头 u32MsgId 即此 ID, 也是 ActorBase:on_msg 收到的第一个参数。
--
-- C++ 侧只定义框架系统消息 (SYS_*) 与类别分段; 业务消息 ID 在此定义。
-- 每个业务消息 ID 在 NAMES 表中登记其 handler 名后缀 (on_<name>_msg)。

xEngine = xEngine or {}

local MSG = {
    -- 框架系统消息 (高 16 位 == 0, 由 C++ CLuaActor::HandleSystemMessage 处理)
    SYS_PING       = 0x00000001,
    SYS_PONG       = 0x00000002,
    SYS_DISCONNECT = 0x00000003,

    -- 业务消息 (高 16 位 == 1, 由 Lua ActorBase:on_msg 分发)
    USER_BEGIN = 0x00010000,
    LOGIN      = 0x00010001,  -- 登录 (gate)
    HEARTBEAT  = 0x00010002,  -- 心跳 (gate)
    AUTH       = 0x00010003,  -- 鉴权 (login)
    PLAYER_SETTLE   = 0x00010004,  -- 战斗结算回传 PlayerActor (battle -> player)
    BATTLE_FINALIZE = 0x00010005,  -- BattleActor 延迟回收 (battle 自投递)
    SPECTATOR_FINALIZE = 0x00010006,  -- SpectatorActor 空闲回收 (spectator 自投递)
    ROOM_START_ASYNC = 0x00010007,  -- Room 异步开局第二阶段 (room 自投递, 消除同步串行排队)
}

-- msg_id → handler 名后缀 映射: on_<name>_msg
-- 处理器定义在 script/actors/<actor>/net_msg/<name>_msg.lua。
MSG.NAMES = {
    [MSG.LOGIN]          = "login",
    [MSG.HEARTBEAT]      = "heartbeat",
    [MSG.AUTH]           = "auth",
    [MSG.PLAYER_SETTLE]  = "player_settle",
    [MSG.BATTLE_FINALIZE] = "battle_finalize",
    [MSG.SPECTATOR_FINALIZE] = "spectator_finalize",
    [MSG.ROOM_START_ASYNC] = "room_start_async",
}

-- 判断是否业务消息 (高 16 位非 0)
function MSG.isUser(msgId)
    return (msgId & 0xFFFF0000) ~= 0
end

xEngine.MsgId = MSG

return MSG
