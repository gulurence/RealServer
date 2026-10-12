-- friend.lua — Friend actor implementation (单例, Global 进程, id=0)
--
-- 客户端业务消息经 Gate 注入发送方 player_id 后, 经 RPC_METHOD_FORWARD_ACTOR
-- 转发到本 Actor 的 on_rpc_forward:
--   wrapped = [8 字节小端 player_id][客户端 protobuf]
-- 用 luacore.si_net_body_cid(wrapped) 取发送方, si_net_body_payload(wrapped) 取负载。
--
-- 好友关系 / 待处理申请存模块级内存表 (单例), 生产可换 CRecordMgr 持久化。

local AT = xEngine.ActorType

-- 好友模块消息 ID (镜像 friend.proto FriendMsgId)
local FriendMsgId = {
    ADD_REQ        = 1,
    ADD_RSP        = 2,
    ADD_APPLY_NTF  = 3,
    ADD_ACCEPT_REQ = 4,
    ADD_ACCEPT_RSP = 5,
    LIST_REQ       = 6,
    LIST_RSP       = 7,
    DEL_REQ        = 8,
    DEL_RSP        = 9,
    ONLINE_NTF     = 10,
    INVITE_REQ     = 11,
    INVITE_NTF     = 12,
}

-- 错误码 (镜像 msg_id.proto ErrorCode; 2001+ 为好友模块私有码)
local EC = {
    NONE            = 0,
    SESSION_INVALID = 1001,
    INTERNAL        = 1999,
    ALREADY_FRIEND  = 2001,
    NOT_FRIEND      = 2002,
}

-- 单例内存态 (存于 xEngine.friend_state, 跨热更保留)
local hot = xEngine.friend_state or {}
xEngine.friend_state = hot
if not hot.friends then hot.friends = {} end
if not hot.pending then hot.pending = {} end
if not hot.names then hot.names = {} end

local friends = hot.friends   -- playerId -> { [friendId] = true }
local pending = hot.pending   -- targetId -> { [applicantId] = true }
local names   = hot.names     -- playerId -> 昵称 (尽力缓存)

local FriendClass = xEngine.Class.define("Friend", xEngine.ActorBase)

function FriendClass:onInstall()
    xEngine.Log.info("[Friend] onInstall")
    return true
end

function FriendClass:onUnInstall()
    xEngine.Log.info("[Friend] onUnInstall")
    return true
end

function FriendClass:data_load(actor_data) return true end
function FriendClass:data_save(save_type) return true end
function FriendClass:onInit()    return true end
function FriendClass:onCreate()  return true end

--------------------------------------------------------------------------------
-- 业务消息总入口 (Gate 转发而来)
--------------------------------------------------------------------------------
function FriendClass:on_rpc_forward(wrapped)
    local senderId  = luacore.si_net_body_cid(wrapped)
    local payload   = luacore.si_net_body_payload(wrapped)
    local smallType = luacore.si_pb_small_type(payload)

    if smallType == FriendMsgId.ADD_REQ then
        return self:handle_add(senderId, payload)
    elseif smallType == FriendMsgId.ADD_ACCEPT_REQ then
        return self:handle_accept(senderId, payload)
    elseif smallType == FriendMsgId.LIST_REQ then
        return self:handle_list(senderId)
    elseif smallType == FriendMsgId.DEL_REQ then
        return self:handle_del(senderId, payload)
    elseif smallType == FriendMsgId.INVITE_REQ then
        return self:handle_invite(senderId, payload)
    end

    xEngine.Log.warn("[Friend] unknown small_type=%d", smallType)
    return ""
end

-- 加好友: 写待处理申请 → ADD_RSP (生产再向 target 推送 ADD_APPLY_NTF)
function FriendClass:handle_add(senderId, payload)
    local targetId = luacore.si_pb_varint_field(payload, 3)

    if friends[senderId] and friends[senderId][targetId] then
        return luacore_friend.si_pb_friend_add_rsp(EC.ALREADY_FRIEND, targetId)
    end

    pending[targetId] = pending[targetId] or {}
    pending[targetId][senderId] = true
    names[senderId] = names[senderId] or ("player_" .. xEngine.Number.intKey(senderId))

    xEngine.Log.info("[Friend] add: %d -> %d (pending)", senderId, targetId)
    -- 生产: 向 targetId 的在线连接推送 ADD_APPLY_NTF(applicantId, applicantName)
    return luacore_friend.si_pb_friend_add_rsp(EC.NONE, targetId)
end

-- 同意/拒绝申请
function FriendClass:handle_accept(senderId, payload)
    local friendId = luacore.si_pb_varint_field(payload, 3)
    local accept   = luacore.si_pb_bool_field(payload, 4)

    if pending[senderId] then pending[senderId][friendId] = nil end

    if accept then
        friends[senderId] = friends[senderId] or {}
        friends[friendId] = friends[friendId] or {}
        friends[senderId][friendId] = true
        friends[friendId][senderId] = true
        xEngine.Log.info("[Friend] accept: %d <-> %d", senderId, friendId)
    end

    return luacore_friend.si_pb_friend_accept_rsp(EC.NONE, friendId)
end

-- 好友列表: "player_id,name,level,online;..."
function FriendClass:handle_list(senderId)
    local parts = {}
    for fid, _ in pairs(friends[senderId] or {}) do
        local name = names[fid] or ("player_" .. xEngine.Number.intKey(fid))
        table.insert(parts, table.concat({
            xEngine.Number.intKey(fid), name, "1", "1",
        }, ","))
    end
    return luacore_friend.si_pb_friend_list_rsp(EC.NONE, table.concat(parts, ";"))
end

-- 删除好友
function FriendClass:handle_del(senderId, payload)
    local friendId = luacore.si_pb_varint_field(payload, 3)
    if friends[senderId] then friends[senderId][friendId] = nil end
    if friends[friendId] then friends[friendId][senderId] = nil end
    xEngine.Log.info("[Friend] del: %d -/-> %d", senderId, friendId)
    return luacore_friend.si_pb_friend_del_rsp(EC.NONE, friendId)
end

-- 邀请好友组队: 无回包, 返回空串 (生产向 friendId 推送 INVITE_NTF)
function FriendClass:handle_invite(senderId, payload)
    local friendId = luacore.si_pb_varint_field(payload, 3)
    xEngine.Log.info("[Friend] invite: %d -> %d", senderId, friendId)
    return ""
end

xEngine.ActorMgr.registerPrototype(AT.FRIEND, FriendClass)
xEngine.ActorMgr.install(AT.FRIEND, 0)
