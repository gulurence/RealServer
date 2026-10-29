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

-- 观战模块小类型 (镜像 spectator.proto SpectatorMsgId)
local SpectatorMsgId = {
    ENTER_REQ = 1, ENTER_RSP = 2, LEAVE_REQ = 3, LEAVE_RSP = 4,
    SNAPSHOT_NTF = 5, FRAME_NTF = 6, END_NTF = 7,
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

--------------------------------------------------------------------------------
-- 观战席位权威操作 (纯 Redis, 在 Gate 本地执行, 不 forward 到 Battle 进程的 SpectatorActor)。
-- SpectatorActor 与 BattleActor 一一对应、同在 Battle 进程, 而 Gate 无直连 Battle 的链接规则
-- (RPC 单跳), 无法 forward 过去; 但观战席位权威名单 battle:spectator:{battleId} 由 Battle 进程
-- 的 BattleActor (每 10 帧 refresh_spectators) 与 CNetBattleGateway (观战直连登录校验) 读取,
-- 故登记/移除只需写 Redis。反向索引 battle:spectator:byplayer:{pid} → battleId 供断线收尾定位。
--------------------------------------------------------------------------------

-- 登记观战席位: 追加 pid 到 battle:spectator:{battleId} (幂等去重), 写反向索引 (TTL 兜底)。
local function spectator_register(battleId, pid)
    local sid  = xEngine.Number.intKey(pid)
    local key  = "battle:spectator:" .. xEngine.Number.intKey(battleId)
    local list = {}
    for v in (xEngine.Redis.Get(key) or ""):gmatch("[^,]+") do
        if v ~= "" and v ~= sid then list[#list + 1] = v end
    end
    list[#list + 1] = sid
    xEngine.Redis.Set(key, table.concat(list, ","))

    local byPlayerKey = "battle:spectator:byplayer:" .. sid
    xEngine.Redis.Set(byPlayerKey, xEngine.Number.intKey(battleId))
    xEngine.Redis.Expire(byPlayerKey, 86400)
end

-- 移除观战席位: 从 battle:spectator:{battleId} 剔除 pid, 清反向索引。
local function spectator_unregister(battleId, pid)
    local sid  = xEngine.Number.intKey(pid)
    local key  = "battle:spectator:" .. xEngine.Number.intKey(battleId)
    local list = {}
    for v in (xEngine.Redis.Get(key) or ""):gmatch("[^,]+") do
        if v ~= "" and v ~= sid then list[#list + 1] = v end
    end
    if #list > 0 then
        xEngine.Redis.Set(key, table.concat(list, ","))
    else
        xEngine.Redis.Del(key)
    end
    xEngine.Redis.Del("battle:spectator:byplayer:" .. sid)
end

-- 从 Redis 重建战斗房花名册 playersStr (镜像 battle.lua players_str / spectator.lua 组装逻辑):
-- 观战者不参与模拟, 但进入战斗场景需要与参战者完全一致的花名册/种子/关卡数据。
-- 返回 playersStr ("pid,name,spawnIndex,primaryGun,level,exp,gold,attack,defense,hp,g0..g5;..."),
--       levelId, difficulty。
local function spectator_build_players_str(battleId)
    local bkey = xEngine.Number.intKey(battleId)
    local roster = xEngine.Redis.Get("battle:roster:" .. bkey) or ""
    if roster == "" then roster = "0" end

    local lvlId, lvlDiff =
        (xEngine.Redis.Get("battle:level:" .. bkey) or ""):match("^(%d+),(%d+)$")
    local levelId    = lvlId and tonumber(lvlId) or 11
    local difficulty = lvlDiff and tonumber(lvlDiff) or 0

    local equipStr = xEngine.Redis.Get("battle:equip:" .. bkey) or ""
    local equipMap = {}
    for rec in (equipStr .. ";"):gmatch("([^;]*);") do
        local pidStr, gunStr, guns = rec:match("^(%d+):(%d+):(.+)$")
        if pidStr and gunStr then
            equipMap[tonumber(pidStr)] = { tonumber(gunStr), guns or "22,22,22,22,22,22" }
        end
    end

    local parts, idx = {}, 0
    for pidStr in (roster .. ","):gmatch("([^,]*),") do
        if pidStr ~= "" then
            local pid = tonumber(pidStr)
            local eq  = equipMap[pid]
            local attrs = xEngine.Role.parseAttrs(pid)
            local name = (attrs.name and attrs.name ~= "") and attrs.name or ("player_" .. pidStr)
            parts[#parts + 1] = table.concat({
                xEngine.Number.intKey(pid), name, tostring(idx),
                tostring(eq and eq[1] or 0), tostring(attrs.level or 1),
                tostring(attrs.exp or 0), tostring(attrs.gold or 0),
                tostring(attrs.attack or 10), tostring(attrs.defense or 0), tostring(attrs.hp or 100),
                (eq and eq[2]) or "22,22,22,22,22,22",
            }, ",")
            idx = idx + 1
        end
    end
    return table.concat(parts, ";"), levelId, difficulty
end

-- 读战斗进程直连地址 battle:host:{battleId} = "ip:listen_port" (room.lua handle_start 写入),
-- 观战者据此直连 Battle 进程握手追帧。返回 hostIp, port。
local function spectator_read_host(battleId)
    local addr = xEngine.Redis.Get("battle:host:" .. xEngine.Number.intKey(battleId)) or ""
    if addr ~= "" then
        local ip, p = addr:match("^([^:]+):(%d+)$")
        if ip and p then return ip, tonumber(p) or 0 end
    end
    return "", 0
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
        if actorType == AT.SPECTATOR then
            -- 观战: SpectatorActor 与 BattleActor 一一对应、同在 Battle 进程, 而 Gate 无
            -- 直连 Battle 的链接规则 (RPC 单跳), 无法 forward 到 SpectatorActor。观战
            -- ENTER/LEAVE 是纯 Redis 席位登记 (权威名单 battle:spectator:{battleId},
            -- 由 Battle 进程 BattleActor/CNetBattleGateway 读取), 故在 Gate 本地处理并回包。
            self:handle_spectate_local(cid, payload)
        else
            self:forward_business(cid, actorType, targetId, payload)
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
-- 观战 ENTER/LEAVE 本地处理 (纯 Redis, 不 forward 到 Battle 进程的 SpectatorActor):
-- 席位权威名单 battle:spectator:{battleId} 由 Battle 进程读取, 故登记/移除只需写 Redis;
-- 回包在本地直接 si_net_send (on_msg 返回值被忽略, 与 forward_business 一致)。
--------------------------------------------------------------------------------
function GateClass:handle_spectate_local(cid, payload)
    local smallType = luacore.si_pb_small_type(payload)
    local resp
    if smallType == SpectatorMsgId.ENTER_REQ then
        resp = self:spectate_enter_local(cid, payload)
    elseif smallType == SpectatorMsgId.LEAVE_REQ then
        resp = self:spectate_leave_local(cid, payload)
    else
        xEngine.Log.warn("[Gate] spectate: unknown small_type=%d", smallType)
        return ""
    end
    if resp and resp ~= "" then
        luacore.si_net_send(cid, resp)
    end
    return resp
end

-- 请求观战: 校验战斗房仍在 → 登记观战席位 → 回 ENTER_RSP (花名册 + 直连 host:port)。
function GateClass:spectate_enter_local(cid, payload)
    local playerId = connPlayer[cid]
    if not playerId then
        xEngine.Log.warn("[Gate] spectate enter: cid=%d no identity, drop", cid)
        return ""
    end

    -- 观战目标可为 battle_id / 房间列表 ID / player_id, 统一解析为 live battle_id
    local inputId  = luacore.si_pb_varint_field(payload, 3)
    local battleId = self:resolve_spectate_battle(inputId)
    if not battleId or battleId <= 0 then
        return luacore_spectate.si_pb_spectate_enter_rsp(EC.BATTLE_NOT_FOUND, battleId or 0, 0, "", 0, 0, "", 0)
    end

    -- 战斗房仍在才允许进入 (roster 空 = 未开战/已结束), 避免渲染假花名册
    local roster = xEngine.Redis.Get("battle:roster:" .. xEngine.Number.intKey(battleId))
    if not roster or roster == "" then
        xEngine.Log.warn("[Gate] spectate enter: battle=%d roster empty, reject", battleId)
        return luacore_spectate.si_pb_spectate_enter_rsp(EC.BATTLE_NOT_FOUND, battleId, 0, "", 0, 0, "", 0)
    end

    -- 登记席位 (battle:spectator:{battleId} + 反向索引), 供 Battle 进程校验直连登录
    spectator_register(battleId, playerId)

    local seed = (xEngine.Number.toInt(battleId) % 0xFFFFFFFF) + 1
    local playersStr, levelId, difficulty = spectator_build_players_str(battleId)
    local hostIp, port = spectator_read_host(battleId)

    xEngine.Log.info("[Gate] spectate enter: battle=%d spectator=%d seed=%d level=%d diff=%d host=%s:%d",
        battleId, playerId, seed, levelId, difficulty, hostIp, port)
    return luacore_spectate.si_pb_spectate_enter_rsp(EC.NONE, battleId, seed, playersStr, levelId, difficulty, hostIp, port)
end

-- 退出观战: 移除观战席位 → 回 LEAVE_RSP。
function GateClass:spectate_leave_local(cid, payload)
    local playerId = connPlayer[cid]
    if not playerId then
        return ""
    end
    local battleId = luacore.si_pb_varint_field(payload, 3)
    -- 客户端可能未带 battle_id, 经反向索引 battle:spectator:byplayer:{pid} 反查
    if not battleId or battleId <= 0 then
        local s = xEngine.Redis.Get("battle:spectator:byplayer:" .. xEngine.Number.intKey(playerId))
        battleId = s and s ~= "" and (tonumber(s) or 0) or 0
    end
    if battleId and battleId > 0 then
        spectator_unregister(battleId, playerId)
    end
    xEngine.Log.info("[Gate] spectate leave: battle=%d spectator=%d", battleId or 0, playerId)
    return luacore_spectate.si_pb_spectate_leave_rsp(EC.NONE)
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

    xEngine.Log.info("[Gate] disconnect cleanup: cid=%d player=%d account=%d",
        cid, playerId or 0, accountId or 0)

    -- 顶号踢旧: 该连接被同账号新登录定向断开 (login.lua 先写 kicked=1 再异步踢)。
    -- 仅清本连接的 cid 映射, 不动 playerConn[playerId] (新连接已接管该玩家) 也不做
    -- 离线收尾 (离线列表/Player 离线/房间战斗分派/session 置 OFFLINE), 避免覆盖新 session;
    -- kicked 标记待新连接完成选角后由 login.lua rpc_select_role 清 0。
    if accountId then
        local kicked = tonumber(xEngine.AccountSession.getField(accountId, "kicked")) or 0
        if kicked == 1 then
            connPlayer[cid] = nil
            connAccount[cid] = nil
            xEngine.Log.info("[Gate] disconnect cleanup: account=%d kicked, skip offline", accountId)
            return
        end
    end

    -- 1) 清理连接映射 (双向), 避免断线后残留连接/身份占用。
    connPlayer[cid] = nil
    connAccount[cid] = nil
    if playerId then
        playerConn[playerId] = nil
    end

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

    -- 观战收尾: 玩家在观战 (battle:spectator:byplayer 存在) → 本地移除观战席位
    -- (观战者断线不经过 LEAVE_REQ; 席位为纯 Redis 权威名单, 无需 forward 到 Battle 进程)。
    local spectateIdStr = xEngine.Redis.Get("battle:spectator:byplayer:" .. pidKey)
    local spectateId    = spectateIdStr and spectateIdStr ~= "" and (tonumber(spectateIdStr) or 0) or 0
    if spectateId > 0 then
        spectator_unregister(spectateId, playerId)
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
