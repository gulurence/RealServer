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
    LOGOUT_REQ      = 7,
    LOGOUT_RSP      = 8,
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
if not hot.connAccount then hot.connAccount = {} end

local connPlayer = hot.connPlayer

-- 玩家 ID → 连接 CID 反向映射 (服务器主动推送 RPC_METHOD_PUSH 时反查连接)
local playerConn = hot.playerConn

-- 连接 CID → 账户 ID 映射 (登录后登记, 断线时写 session offline)
local connAccount = hot.connAccount

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
    SPECTATOR_LEAVE   = 3,   -- SpectatorMsgId.LEAVE_REQ (观战断线移除观战席位)
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
        elseif smallType == LoginMsgId.LOGOUT_REQ then
            self:handle_logout(cid, payload)
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
            -- 惰性创建: 仅客户端发起观战请求时才创建 SpectatorActor (与 BattleActor
            -- 经 Redis 同步), 避免每场战斗常驻一个空转的观战实例。
            self:ensure_spectator(targetId)
        end
        local resp = self:forward_business(cid, actorType, targetId, payload)
        if (resp == nil or resp == "") and actorType == AT.SPECTATOR then
            -- 观战目标战斗房不存在 (SpectatorActor 未创建): 回错误码, 避免客户端静默挂起
            xEngine.Log.warn("[Gate] spectate: target=%d not found, reply error", targetId)
            luacore.si_net_send(cid,
                luacore_spectate.si_pb_spectate_enter_rsp(EC.BATTLE_NOT_FOUND, targetId, 0, "", 0, 0, "", 0))
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
-- 惰性创建观战实例: 仅在客户端首次发起观战请求 (且战斗房仍在) 时创建 SpectatorActor。
-- SpectatorActor 与 BattleActor 通过 Redis (battle:roster/level/equip/spectator) 同步状态,
-- 不需要同进程; 已存在则复用, 不重复创建。
--------------------------------------------------------------------------------
function GateClass:ensure_spectator(battleId)
    if not battleId or battleId <= 0 then return false end
    local bkey = xEngine.Number.intKey(battleId)

    -- 战斗房仍在才创建 (roster 空 = 未开战/已结束), 避免残留指向已死战斗房的空转实例
    local roster = xEngine.Redis.Get("battle:roster:" .. bkey)
    if not roster or roster == "" then
        xEngine.Log.warn("[Gate] ensure_spectator: battle=%d not active, skip create", battleId)
        return false
    end

    -- 已存在则复用: 读注册 Hash cluster:actor:204:{id} 的 pid 字段 (与 C++ ResolveActorProcess
    -- 同一权威源), 跨进程可辨存在性, 避免重复创建。
    local pid = xEngine.Redis.HGet("cluster:actor:" .. AT.SPECTATOR .. ":" .. bkey, "pid")
    if pid ~= "" then
        return true
    end

    -- 创建锁: 并发观战同一新战斗时只允许一个请求创建 (SETNXEX 原子占锁), 抢不到则说明
    -- 其它请求正在创建, 本次不重复建 (后续 forward 兜底, 客户端可重试观战)。
    local lockKey = "battle:spectator:create_lock:" .. bkey
    if not xEngine.Redis.SetNxEx(lockKey, "1", 3) then
        xEngine.Log.info("[Gate] ensure_spectator: battle=%d create in progress by another request", battleId)
        return false
    end

    local s = xEngine.ActorCreator.balanceCreate(AT.SPECTATOR, battleId)
    xEngine.Redis.Del(lockKey)
    if s then
        xEngine.Log.info("[Gate] ensure_spectator: lazily created spectator battle=%d", battleId)
        return true
    end
    xEngine.Log.error("[Gate] ensure_spectator: create failed battle=%d", battleId)
    return false
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

    -- 登录成功 (error_code==NONE): 补写 session 的连接归属 (gate_process/cid),
    -- 供下次并发登录「踢旧」定向断开; 并登记 connAccount 供断线时置 offline。
    -- 排队响应 (error_code==ERR_QUEUED) 不写 session。
    local errCode = luacore.si_pb_login_rsp_error_code(resp)
    local accountId = luacore.si_pb_login_rsp_account_id(resp)
    if errCode == EC.NONE and accountId and accountId > 0 then
        connAccount[cid] = accountId
        xEngine.AccountSession.set(accountId, {
            gate_process = luacore.si_get_process_id(),
            cid          = cid,
            last_seen    = luacore.si_time_cur_sec(),
        })
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
-- 统一下线收尾 (断线 + 主动登出共用): 清理连接映射 + 账户 session + 下线列表 +
-- 房间/战斗分派 + Player 离线。cid = 连接 ID。
--------------------------------------------------------------------------------
function GateClass:do_disconnect_cleanup(cid)
    local playerId = connPlayer[cid]
    local accountId = connAccount[cid]

    -- 1) 清理连接映射 (双向), 避免断线后残留连接/身份占用。
    connPlayer[cid] = nil
    connAccount[cid] = nil
    if playerId then
        playerConn[playerId] = nil
    end

    xEngine.Log.info("[Gate] disconnect cleanup: cid=%d player=%d account=%d",
        cid, playerId or 0, accountId or 0)

    -- 尚未选角 (无玩家身份): 仅清连接 + session 置 offline, 无业务收尾。
    if not playerId then
        if accountId then
            xEngine.AccountSession.set(accountId, {
                state = xEngine.SessionState.OFFLINE,
                kicked = 0,
                last_seen = luacore.si_time_cur_sec(),
            })
        end
        return
    end

    -- 2) 下线列表: 下线即入列 (offline:players) 并记录下线时间 (24h 兜底 TTL)。
    --    语义为「Gate 视角的离线」: 战斗中仅 Gate 连接断开的玩家也在此列, 由
    --    battle.handle_enter 重连或重新选角时移除 (见 login.lua / battle.lua)。
    local pidKey = xEngine.Number.intKey(playerId)
    xEngine.Redis.SAdd("offline:players", pidKey)
    xEngine.Redis.Set("offline:ts:" .. pidKey, tostring(luacore.si_time_cur_sec()))
    xEngine.Redis.Expire("offline:ts:" .. pidKey, 86400)

    -- 3) 战斗/房间分派: 玩家在战斗中 (battle:byplayer 存在) → 通知战斗房标记离线,
    --    角色原地等重连 (req2/req8), 组队房成员关系保留; 否则按离开处理 (大厅断线即离房)。
    local battleIdStr = xEngine.Redis.Get("battle:byplayer:" .. pidKey)
    local inBattle  = battleIdStr and battleIdStr ~= ""
    local battleId  = inBattle and (tonumber(battleIdStr) or 0) or 0

    -- 账户 session 状态 (登录→离线闭环): 战斗中断线保留 IN_BATTLE (可重连, 不置 OFFLINE),
    -- 并缓存战斗房登录信息 (ip/port/key) 供重登查询; 其余场景置 OFFLINE。Gate 只负责在线
    -- 状态相关逻辑, 战斗逻辑由 Battle 进程独立处理 (分离)。
    if accountId then
        if inBattle then
            local battleHost = xEngine.Redis.Get("battle:host:" .. xEngine.Number.intKey(battleId)) or ""
            local battleAuth = xEngine.Redis.Get("battle:auth:" .. xEngine.Number.intKey(battleId)) or ""
            xEngine.AccountSession.set(accountId, {
                state       = xEngine.SessionState.IN_BATTLE,
                battle_id   = xEngine.Number.intKey(battleId),
                battle_host = battleHost,
                battle_auth = battleAuth,
                last_seen   = luacore.si_time_cur_sec(),
            })
        else
            xEngine.AccountSession.set(accountId, {
                state = xEngine.SessionState.OFFLINE,
                kicked = 0,
                last_seen = luacore.si_time_cur_sec(),
            })
        end
    end

    if inBattle then
        luacore.si_forward_rpc(AT.BATTLE, battleId,
            luacore.si_net_body_wrap(playerId, encode_small_type(DISCONNECT_SMALL.BATTLE_DISCONNECT)))
    else
        luacore.si_forward_rpc(AT.ROOM, 0,
            luacore.si_net_body_wrap(playerId, encode_small_type(DISCONNECT_SMALL.ROOM_LEAVE)))
    end

    -- 观战收尾: 玩家在观战 (battle:spectator:byplayer 存在) → 通知 SpectatorActor 移除观战席位
    -- (观战者断线不经过 LEAVE_REQ, 走 handle_leave 统一移除 + 空席触发空闲回收)。
    local spectateIdStr = xEngine.Redis.Get("battle:spectator:byplayer:" .. pidKey)
    local spectateId    = spectateIdStr and spectateIdStr ~= "" and (tonumber(spectateIdStr) or 0) or 0
    if spectateId > 0 then
        luacore.si_forward_rpc(AT.SPECTATOR, spectateId,
            luacore.si_net_body_wrap(playerId, encode_small_type(DISCONNECT_SMALL.SPECTATOR_LEAVE)))
    end

    -- 4) Player Actor 状态置离线: 合成 DISCONNECT 通知, Player 落 frame 状态字段 online=0。
    luacore.si_forward_rpc(AT.PLAYER, playerId,
        luacore.si_net_body_wrap(playerId, encode_small_type(DISCONNECT_SMALL.PLAYER_OFFLINE)))
end

--------------------------------------------------------------------------------
-- 客户端断线统一收尾 (C++ CNetGateway::OnDisconnect → SYS_DISCONNECT → 本方法)
--   cidBody = [8 字节小端 CID]
--------------------------------------------------------------------------------
function GateClass:on_rpc_disconnect(cidBody)
    local cid = luacore.si_net_body_cid(cidBody)
    self:do_disconnect_cleanup(cid)
    return ""
end

--------------------------------------------------------------------------------
-- 主动登出 (客户端 LOGIN_MSG_LOGOUT_REQ): 与断线同样的收尾, 但由客户端显式触发,
-- 服务器立即下线 (无需等 TCP 断开), 收尾完成后回 LOGOUT_RSP 确认。
--------------------------------------------------------------------------------
function GateClass:handle_logout(cid, payload)
    xEngine.Log.info("[Gate] logout: cid=%d player=%d", cid, connPlayer[cid] or 0)
    self:do_disconnect_cleanup(cid)
    luacore.si_net_send(cid, luacore.si_pb_logout_rsp(EC.NONE))
    return ""
end

xEngine.ActorMgr.registerPrototype(AT.GATE, GateClass)
xEngine.ActorMgr.install(AT.GATE, 0)
