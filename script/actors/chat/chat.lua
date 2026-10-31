--[[
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: chat.lua
      Purpose:   聊天 Actor 实现（单例，Global 进程）

      ChatClass 经 on_rpc_forward 处理 SEND_REQ/HISTORY_REQ，按频道类型与频道 id 把历史消息缓存在 xEngine.chat_state（跨热更保留）。
      handle_send 写入历史并实时广播 CHAT_NTF，handle_history 返回频道历史；历史以逗号/分号拼串编码，含定界符的 content 会被截断。
]]

-- chat.lua — Chat actor implementation (单例, Global 进程, id=0)
--
-- 客户端业务消息经 Gate 注入发送方 player_id 后, 经 RPC_METHOD_FORWARD_ACTOR
-- 转发到本 Actor 的 on_rpc_forward。历史消息按 "频道类型:频道id" 缓存在内存。
--
-- 注意: 历史消息以 "sender_id,sender_name,content,timestamp;" 拼串编码,
-- content 含 ',' 或 ';' 会截断 —— 生产应改用 JSON/base64 编码再入库。

local AT = xEngine.ActorType

-- 聊天模块消息 ID (镜像 chat.proto ChatMsgId)
local ChatMsgId = {
    SEND_REQ    = 1,
    SEND_RSP    = 2,
    NTF         = 3,
    HISTORY_REQ = 4,
    HISTORY_RSP = 5,
}

-- 频道类型 (镜像 chat.proto ChatChannelType)
local CHANNEL = { NONE = 0, ROOM = 1, BATTLE = 2 }

-- 错误码
local EC = { NONE = 0, SESSION_INVALID = 1001, INTERNAL = 1999 }

local MAX_HISTORY = 50

-- 单例内存态 (存于 xEngine.chat_state, 跨热更保留):
-- key = "channelType:channelId" -> { {senderId, senderName, content, timestamp}, ... }
local hot = xEngine.chat_state or {}
xEngine.chat_state = hot
if not hot.history then hot.history = {} end
local history = hot.history

local ChatClass = xEngine.Class.define("Chat", xEngine.ActorBase)

function ChatClass:onInstall()
    xEngine.Log.info("[Chat] onInstall")
    return true
end

function ChatClass:onUnInstall()
    xEngine.Log.info("[Chat] onUnInstall")
    return true
end

function ChatClass:data_load(actor_data) return true end
function ChatClass:data_save(save_type) return true end
function ChatClass:onInit()    return true end
function ChatClass:onCreate()  return true end

--------------------------------------------------------------------------------
-- 业务消息总入口
--------------------------------------------------------------------------------
function ChatClass:on_rpc_forward(wrapped)
    local senderId  = xEngine.Number.toInt(luacore.si_net_body_cid(wrapped))
    local payload   = luacore.si_net_body_payload(wrapped)
    local smallType = luacore.si_pb_small_type(payload)

    if smallType == ChatMsgId.SEND_REQ then
        return self:handle_send(senderId, payload)
    elseif smallType == ChatMsgId.HISTORY_REQ then
        return self:handle_history(senderId, payload)
    end

    xEngine.Log.warn("[Chat] unknown small_type=%d", smallType)
    return ""
end

function ChatClass:channel_key(channelType, channelId)
    return xEngine.Number.intKey(channelType) .. ":" .. xEngine.Number.intKey(channelId)
end

function ChatClass:handle_send(senderId, payload)
    local channelType = xEngine.Number.toInt(luacore.si_pb_varint_field(payload, 3))
    local channelId   = xEngine.Number.toInt(luacore.si_pb_varint_field(payload, 4))
    local content     = luacore.si_pb_string_field(payload, 5)

    local senderName = "player_" .. xEngine.Number.intKey(senderId)
    local timestamp  = xEngine.Number.toInt(luacore.si_time_cur_sec())

    local key = self:channel_key(channelType, channelId)
    local list = history[key]
    if not list then list = {}; history[key] = list end
    table.insert(list, { senderId, senderName, content, timestamp })
    if #list > MAX_HISTORY then table.remove(list, 1) end

    xEngine.Log.info("[Chat] send: c=%s from=%d len=%d", key, senderId, #content)
    -- 实时广播: 向所有客户端推送聊天 NTF (playerId=0 哨兵), 客户端按
    -- channel_type/channel_id 过滤只显示自己所在频道; 生产如需严格隔离可改走
    -- Redis room:members 定向推送给房内成员。
    local ntf = luacore_chat.si_pb_chat_ntf(channelType, channelId, senderId, senderName, content)
    luacore.si_push_rpc(0, ntf)
    return luacore_chat.si_pb_chat_send_rsp(EC.NONE)
end

function ChatClass:handle_history(senderId, payload)
    local channelType = xEngine.Number.toInt(luacore.si_pb_varint_field(payload, 3))
    local channelId   = xEngine.Number.toInt(luacore.si_pb_varint_field(payload, 4))
    local key = self:channel_key(channelType, channelId)

    local parts = {}
    for _, msg in ipairs(history[key] or {}) do
        table.insert(parts, table.concat({
            xEngine.Number.intKey(msg[1]), msg[2], msg[3], xEngine.Number.intKey(msg[4]),
        }, ","))
    end
    return luacore_chat.si_pb_chat_history_rsp(EC.NONE, channelType, channelId, table.concat(parts, ";"))
end

xEngine.ActorMgr.registerPrototype(AT.CHAT, ChatClass)
xEngine.ActorMgr.install(AT.CHAT, 0)
