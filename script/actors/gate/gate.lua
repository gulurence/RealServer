-- gate.lua — Gate actor implementation (客户端接入 + 登录链路编排)
--
-- 客户端 → Gate: asio TCP 长连接, 帧 = [uint32 len][protobuf body]。
-- C++ CNetGateway 解析 body 的 BigType/SmallType, 组装 msgId = (BigType<<16)|SmallType,
-- 并以 [8 字节 CID][protobuf body] 投递给本 Actor 的 on_msg。
--
-- on_msg 收到登录/选角请求后:
--   1. 转发给 Login 进程 (Gate→Login RPC, si_login_rpc)
--   2. Login 进程校验 Session + 选角创建 Player 到 Game (产生 Gate→Login→Game 三级链路)
--   3. Login 返回回包 protobuf, Gate 透传给客户端 (si_net_send)
local AT = xEngine.ActorType

-- 登录模块消息 ID (SmallType 路由字段, 镜像 login.proto 的 LoginMsgId 枚举)
local LoginMsgId = {
    LOGIN_REQ       = 1,
    LOGIN_RSP       = 2,
    SELECT_ROLE_REQ = 3,
    SELECT_ROLE_RSP = 4,
    CREATE_ROLE_REQ = 5,
    CREATE_ROLE_RSP = 6,
}

-- 通用错误码 (镜像 msg_id.proto 的 ErrorCode 枚举)
local EC = {
    NONE                 = 0,
    SESSION_INVALID      = 1001,
    ACCOUNT_NOT_FOUND    = 1002,
    ROLE_NOT_FOUND       = 1003,
    NO_AVAILABLE_PROCESS = 1004,
    INTERNAL             = 1999,
    BATTLE_NOT_FOUND     = 2201,  -- 观战目标战斗房不存在 (SpectatorActor 未创建/已结束)
}

-- 登录链路 RPC op (镜像 CActorLoadManager::LoginRemote 的 op 编码)
local LOGIN_OP = {
    LOGIN       = 1,   -- 登录校验 (转发给 Login 进程)
    SELECT_ROLE = 2,   -- 选角 (转发给 Login 进程)
    CREATE_ROLE = 3,   -- 创角 (转发给 Login 进程)
}

-- 连接 → 玩家 ID 映射 (选角成功后登记, 业务消息转发时注入发送方身份)
-- 值由服务器会话权威维护, 客户端业务消息无需自报身份。
-- 单例内存态存于 xEngine.gate_state (跨热更保留, 在线连接不中断)。
local hot = xEngine.gate_state or {}
xEngine.gate_state = hot
if not hot.connPlayer then hot.connPlayer = {} end
if not hot.playerConn then hot.playerConn = {} end

local connPlayer = hot.connPlayer

-- 玩家 ID → 连接 CID 反向映射 (服务器主动推送 RPC_METHOD_PUSH 时反查连接)
local playerConn = hot.playerConn

-- 业务消息大类型 → 目标 Actor 类型 (镜像 msg_id.proto PbID.ActorType)
-- 单例 (Friend/Room/Chat) 目标 id=0; 多实例 (Battle/Spectator) 目标 id=消息内 battle_id。
local SINGLETON_BIG = {
    [102] = AT.FRIEND,   -- friend
    [103] = AT.ROOM,     -- room
    [104] = AT.CHAT,     -- chat
    [105] = AT.CONFIG,   -- config (配置热更, 广播用 playerId==0 哨兵)
}
local BATTLE_BIG = {
    [203] = AT.BATTLE,       -- battle
    [204] = AT.SPECTATOR,    -- spectator
}

-- 断线收尾用到的跨模块小类型 (镜像 room.lua RoomMsgId / player.lua PlayerMsgId)。
-- Gate 合成服务端内部消息时只编码 small_type 字段 (目标 Actor 的 on_rpc_forward
-- 仅读 si_pb_small_type), 不携带客户端 protobuf。
local DISCONNECT_SMALL = {
    ROOM_LEAVE        = 7,   -- RoomMsgId.LEAVE_REQ (roomId 缺省=0, Room 按 playerRoom 反查)
    PLAYER_OFFLINE    = 6,   -- PlayerMsgId.DISCONNECT (Player 落状态字段 online=0)
    BATTLE_DISCONNECT = 11,  -- BattleMsgId.DISCONNECT (战斗中断线标记离线, 角色原地等重连)
}

-- 编码仅含 small_type(field 2, varint) 的最小 protobuf 负载: tag = (2<<3)|0 = 0x10。
local function encode_small_type(smallType)
    return string.char(0x10, smallType % 0x100)
end

local GateClass = xEngine.Class.define("Gate", xEngine.ActorBase)

function GateClass:onInstall()
    xEngine.Log.info("[Gate] onInstall — connections ready")
    return true
end

function GateClass:onUnInstall()
    xEngine.Log.info("[Gate] onUnInstall")
    return true
end

function GateClass:data_load(actor_data)
    return true
end

function GateClass:data_save(save_type)
    return true
end

function GateClass:on_event(event_enum)
end

function GateClass:onInit()
    return true
end

function GateClass:onCreate()
    return true
end

--------------------------------------------------------------------------------
-- 客户端消息分发 (由 CLuaActor::OnMessage 以 on_msg(self, msgId, body) 调用)
--   msgId  = (BigType << 16) | SmallType
--   body   = [8 字节 CID][protobuf 负载]
--------------------------------------------------------------------------------
function GateClass:on_msg(msgId, rawBody)
    local smallType = msgId & 0xFFFF
    local bigType   = (msgId >> 16) & 0xFFFF
    local cid       = luacore.si_net_body_cid(rawBody)
    local payload   = luacore.si_net_body_payload(rawBody)

    xEngine.Log.info("[Gate] on_msg cid=%d msgId=%d big=%d small=%d",
        cid, msgId, bigType, smallType)

    if bigType == AT.LOGIN then
        if smallType == LoginMsgId.LOGIN_REQ then
            self:handle_login(cid, payload)
        elseif smallType == LoginMsgId.SELECT_ROLE_REQ then
            self:handle_select_role(cid, payload)
        elseif smallType == LoginMsgId.CREATE_ROLE_REQ then
            self:handle_create_role(cid, payload)
        else
            xEngine.Log.warn("[Gate] unknown login small_type=%d", smallType)
        end
    elseif SINGLETON_BIG[bigType] then
        self:forward_business(cid, SINGLETON_BIG[bigType], 0, payload)
    elseif BATTLE_BIG[bigType] then
        -- 多实例 Actor: 目标 id = 消息内 battle_id (field 3)
        local targetId = luacore.si_pb_varint_field(payload, 3)
        local actorType = BATTLE_BIG[bigType]
        -- 观战目标可为 battle_id / 房间列表 ID / player_id, 统一解析为 live battle_id
        if actorType == AT.SPECTATOR then
            targetId = self:resolve_spectate_battle(targetId)
        end
        local resp = self:forward_business(cid, actorType, targetId, payload)
        if (resp == nil or resp == "") and actorType == AT.SPECTATOR then
            -- 观战目标战斗房不存在 (SpectatorActor 未创建): 回错误码, 避免客户端静默挂起
            xEngine.Log.warn("[Gate] spectate: target=%d not found, reply error", targetId)
            luacore.si_net_send(cid,
                luacore_spectate.si_pb_spectate_enter_rsp(EC.BATTLE_NOT_FOUND, targetId, 0, "", 0, 0))
        end
    elseif bigType == AT.PLAYER then
        -- 多实例 Actor: 目标 id = 发送方自身 player_id (选角后已登记 connPlayer[cid])
        local playerId = connPlayer[cid]
        if not playerId then
            xEngine.Log.warn("[Gate] player big_type without identity cid=%d, drop", cid)
        else
            self:forward_business(cid, AT.PLAYER, playerId, payload)
        end
    else
        xEngine.Log.warn("[Gate] unknown big_type=%d small_type=%d", bigType, smallType)
    end
end

--------------------------------------------------------------------------------
-- 观战目标解析: 客户端输入的观战 ID 可为以下任一种, 统一解析为 live battle_id:
--   1. battle_id (战斗房自身 id, 如 5000)
--   2. 房间列表 ID (room_id, 如 1000)  —— 经 battle:byroom:{roomId} 反查
--   3. 玩家 ID (player_id)             —— 经 battle:byplayer:{pid} 反查
-- 解析不到映射则原样返回 (视为 battle_id), 交由后续 SpectatorActor 转发失败兜底回错。
-- 映射由 room.lua handle_start 写入、battle.lua 结束战斗时清理。
--------------------------------------------------------------------------------
function GateClass:resolve_spectate_battle(inputId)
    if not inputId or inputId <= 0 then return inputId end
    local key = xEngine.Number.intKey(inputId)

    local byRoom = xEngine.Redis.Get("battle:byroom:" .. key)
    if byRoom and byRoom ~= "" then
        return tonumber(byRoom) or inputId
    end

    local byPlayer = xEngine.Redis.Get("battle:byplayer:" .. key)
    if byPlayer and byPlayer ~= "" then
        return tonumber(byPlayer) or inputId
    end

    return inputId
end

--------------------------------------------------------------------------------
-- 通用业务转发: 注入发送方 player_id, RPC 转发给目标 Actor, 回包透传
-- 目标 Actor 用 si_net_body_cid(wrapped) 还原发送方, si_net_body_payload(wrapped)
-- 还原客户端 protobuf 负载。
--------------------------------------------------------------------------------
function GateClass:forward_business(cid, actorType, actorId, payload)
    local playerId = connPlayer[cid]
    if not playerId then
        xEngine.Log.warn("[Gate] forward_business: cid=%d no player identity, drop", cid)
        return nil
    end

    local wrapped = luacore.si_net_body_wrap(playerId, payload)
    local resp = luacore.si_forward_rpc(actorType, actorId, wrapped)

    if resp and resp ~= "" then
        luacore.si_net_send(cid, resp)
        return resp
    end
    return nil
end

--------------------------------------------------------------------------------
-- 登录请求: 转发给 Login 进程 (Gate→Login RPC), 回包透传
--------------------------------------------------------------------------------
function GateClass:handle_login(cid, payload)
    local resp = luacore.si_login_rpc(LOGIN_OP.LOGIN, payload)

    if resp == nil or resp == "" then
        xEngine.Log.error("[Gate] login: Login RPC failed")
        luacore.si_net_send(cid, luacore.si_pb_login_rsp(EC.SESSION_INVALID, 0, ""))
        return
    end

    luacore.si_net_send(cid, resp)
end

--------------------------------------------------------------------------------
-- 选角请求: 转发给 Login 进程 (Gate→Login RPC), Login 校验后创建 Player 到 Game
--------------------------------------------------------------------------------
function GateClass:handle_select_role(cid, payload)
    local roleId = luacore.si_pb_select_role_role_id(payload)
    local resp = luacore.si_login_rpc(LOGIN_OP.SELECT_ROLE, payload)

    if resp == nil or resp == "" then
        xEngine.Log.error("[Gate] select_role: Login RPC failed role=%d", roleId)
        luacore.si_net_send(cid, luacore.si_pb_select_role_rsp(EC.NO_AVAILABLE_PROCESS, 0, roleId, ""))
        return
    end

    -- 选角成功: player_id == role_id (见 login.lua rpc_select_role), 登记连接身份
    connPlayer[cid] = roleId
    playerConn[roleId] = cid
    luacore.si_net_send(cid, resp)
end

--------------------------------------------------------------------------------
-- 创角请求: 转发给 Login 进程 (Gate→Login RPC), 回包透传
--------------------------------------------------------------------------------
function GateClass:handle_create_role(cid, payload)
    local resp = luacore.si_login_rpc(LOGIN_OP.CREATE_ROLE, payload)

    if resp == nil or resp == "" then
        xEngine.Log.error("[Gate] create_role: Login RPC failed")
        luacore.si_net_send(cid, luacore.si_pb_create_role_rsp(EC.INTERNAL, 0, "", 0, 0))
        return
    end

    luacore.si_net_send(cid, resp)
end

--------------------------------------------------------------------------------
-- 服务器主动推送 (业务 Actor → Gate): rawBody = [8 字节小端 player_id][客户端 protobuf]
-- 由 C++ RPC_METHOD_PUSH handler 调用, 按 player_id 反查连接并下发客户端。
--------------------------------------------------------------------------------
function GateClass:on_rpc_push(rawBody)
    local playerId = luacore.si_net_body_cid(rawBody)
    local payload  = luacore.si_net_body_payload(rawBody)

    -- playerId==0 哨兵 = 全员广播 (Config 热更 CHANGED_NTF 等), 遍历已登记连接下发。
    if playerId == 0 then
        local n = 0
        for pid, cid in pairs(playerConn) do
            luacore.si_net_send(cid, payload)
            n = n + 1
        end
        xEngine.Log.info("[Gate] push: broadcast to %d conns", n)
        return ""
    end

    local cid = playerConn[playerId]
    if not cid then
        xEngine.Log.warn("[Gate] push: player=%d no connection, drop", playerId)
        return ""
    end

    luacore.si_net_send(cid, payload)
    return ""
end

--------------------------------------------------------------------------------
-- 客户端断线统一收尾 (C++ CNetGateway::OnDisconnect → SYS_DISCONNECT → 本方法)
--   cidBody = [8 字节小端 CID]
-- 职责: 1) 清理连接映射; 2) 房间按离开处理; 3) Player Actor 状态置离线。
--------------------------------------------------------------------------------
function GateClass:on_rpc_disconnect(cidBody)
    local cid = luacore.si_net_body_cid(cidBody)
    local playerId = connPlayer[cid]

    -- 1) 清理连接映射 (双向), 避免断线后残留连接/身份占用。
    connPlayer[cid] = nil
    if playerId then
        playerConn[playerId] = nil
    end

    xEngine.Log.info("[Gate] disconnect: cid=%d player=%d", cid, playerId or 0)

    -- 尚未选角 (无玩家身份): 仅清连接, 无业务收尾。
    if not playerId then
        return ""
    end

    -- 2) 战斗/房间分派: 玩家在战斗中 (battle:byplayer 存在) → 通知战斗房标记离线,
    --    角色原地等重连 (req2/req8), 组队房成员关系保留; 否则按离开处理 (大厅断线即离房)。
    local battleIdStr = xEngine.Redis.Get("battle:byplayer:" .. xEngine.Number.intKey(playerId))
    if battleIdStr and battleIdStr ~= "" then
        local battleId = tonumber(battleIdStr) or 0
        luacore.si_forward_rpc(AT.BATTLE, battleId,
            luacore.si_net_body_wrap(playerId, encode_small_type(DISCONNECT_SMALL.BATTLE_DISCONNECT)))
    else
        luacore.si_forward_rpc(AT.ROOM, 0,
            luacore.si_net_body_wrap(playerId, encode_small_type(DISCONNECT_SMALL.ROOM_LEAVE)))
    end

    -- 3) Player Actor 状态置离线: 合成 DISCONNECT 通知, Player 落 frame 状态字段 online=0。
    luacore.si_forward_rpc(AT.PLAYER, playerId,
        luacore.si_net_body_wrap(playerId, encode_small_type(DISCONNECT_SMALL.PLAYER_OFFLINE)))

    return ""
end

xEngine.ActorMgr.registerPrototype(AT.GATE, GateClass)
xEngine.ActorMgr.install(AT.GATE, 0)
