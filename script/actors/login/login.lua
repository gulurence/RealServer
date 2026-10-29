-- login.lua — Login actor implementation
local AT = xEngine.ActorType

-- 登录链路 RPC op (镜像 CActorLoadManager::LoginRemote 的 op 编码)
local LOGIN_OP = {
    LOGIN       = 1,   -- 登录校验 (校验 Session → 回 LoginRsp)
    SELECT_ROLE = 2,   -- 选角 (创建 Player Actor → 回 SelectRoleRsp)
    CREATE_ROLE = 3,   -- 创角 (写角色列表 → 回 CreateRoleRsp)
}

-- 通用错误码 (镜像 msg_id.proto 的 ErrorCode 枚举)
local EC = {
    NONE                 = 0,
    SESSION_INVALID      = 1001,
    ACCOUNT_NOT_FOUND    = 1002,
    ROLE_NOT_FOUND       = 1003,
    NO_AVAILABLE_PROCESS = 1004,
    ROLE_NAME_TAKEN      = 1005,
    ROLE_NAME_INVALID    = 1006,
    QUEUED               = 1007,  -- 登录排队中 (集群 PlayerActor 满载)
    INTERNAL             = 1999,
}

-- 登录中状态 (LOGGED_IN/ROLE_SELECTED) 的陈旧判定阈值 (秒):
-- 同账号并发登录时, 旧登录若在此时长内仍处于「登录中」(尚未进入游戏), 拒绝新登录;
-- 超过此时长视为陈旧登录, 允许新登录覆盖。须小于 session TTL(30s), 否则 session 键先过期。
local LOGIN_TIMEOUT_SEC = 15

-- 编码仅含 small_type(field 2, varint) 的最小 protobuf 负载: tag = (2<<3)|0 = 0x10。
-- (与 gate.lua 同款 helper; 供选角上线时通知 Player Actor 恢复 online=1 用)
local function encode_small_type(smallType)
    return string.char(0x10, smallType % 0x100)
end

-- PlayerMsgId.ONLINE (镜像 player.lua): 服务端内部通知 Player 置 frame 状态 online=1
local PLAYER_ONLINE = 7

local LoginClass = xEngine.Class.define("Login", xEngine.ActorBase)

function LoginClass:onInstall()
    xEngine.Log.info("[Login] onInstall")
    return true
end

function LoginClass:onUnInstall()
    xEngine.Log.info("[Login] onUnInstall")
    return true
end

function LoginClass:data_load(actor_data)
    return true
end

function LoginClass:data_save(save_type)
    return true
end

function LoginClass:on_event(event_enum)
end

function LoginClass:onInit()
    return true
end

function LoginClass:onCreate()
    return true
end

--------------------------------------------------------------------------------
-- 登录链路 RPC (Gate→Login): reqBody = [1 字节 op][客户端 protobuf 负载]
-- 由 C++ ActorRpcHandlers 的 RPC_METHOD_LOGIN 处理器调用, 返回客户端回包 protobuf。
--------------------------------------------------------------------------------
function LoginClass:on_rpc_login(reqBody)
    local op      = reqBody:byte(1)
    local payload = reqBody:sub(2)

    if op == LOGIN_OP.LOGIN then
        return self:rpc_login(payload)
    elseif op == LOGIN_OP.SELECT_ROLE then
        return self:rpc_select_role(payload)
    elseif op == LOGIN_OP.CREATE_ROLE then
        return self:rpc_create_role(payload)
    end

    xEngine.Log.warn("[Login] on_rpc_login: unknown op=%d", op)
    return ""
end

-- 进程级缓存 player_actor_cap: 每次登录都 HGet 一次 → 缓存 5s (登录热路径省 1 往返)。
-- 单例 Login (id=0) 每进程仅一份, 模块级缓存即每进程一份; Lua 热更重载自然失效。
local capCache = { value = 0, expireAt = 0 }
local CAP_CACHE_TTL = 5

-- 读取 PlayerActor 数量上限 (cluster:config:game 的 player_actor_cap; 0=不限)
function LoginClass:get_player_actor_cap()
    local now = luacore.si_time_cur_sec()
    if capCache.expireAt > now then
        return capCache.value
    end
    local v = xEngine.Redis.HGet("cluster:config:game", "player_actor_cap")
    capCache.value = tonumber(v) or 0
    capCache.expireAt = now + CAP_CACHE_TTL
    return capCache.value
end

-- 登录: 校验 Session → session 同步(踢旧允许新) → 排队判定 → 返回 LoginRsp
function LoginClass:rpc_login(payload)
    local token     = luacore.si_pb_login_req_token(payload)
    local accountId = xEngine.Session.validate(token)

    if not accountId then
        xEngine.Log.warn("[Login] login: invalid session token")
        return luacore.si_pb_login_rsp(EC.SESSION_INVALID, 0, "")
    end

    -- 每账号登录短锁: 串行化同一账户在不同 LoginActor 上的并发登录
    if not xEngine.AccountSession.acquireLock(accountId) then
        xEngine.Log.warn("[Login] login: account=%d login in progress, retry", accountId)
        return luacore.si_pb_login_rsp(EC.SESSION_INVALID, 0, "")
    end

    -- 登录排队: 集群 PlayerActor 满载时入队, 返回排队位置 + 预计等待 (平均等待时间)。
    -- 排队阶段不踢旧 (旧连接仍在线), 待放行时再由下方「顶号/超时判定」处理。
    local cap = self:get_player_actor_cap()
    if cap > 0 then
        local count = tonumber(xEngine.Redis.Get("cluster:load:player_count")) or 0
        if count >= cap then
            xEngine.LoginQueue.enqueue(accountId)
            local position = xEngine.LoginQueue.position(accountId)
            local estWait = xEngine.LoginQueue.estWait(position)
            xEngine.AccountSession.releaseLock(accountId)
            xEngine.Log.info("[Login] login: account=%d queued pos=%d est=%ds (count=%d cap=%d)",
                accountId, position, estWait, count, cap)
            return luacore.si_pb_login_rsp_queued(accountId, position, estWait)
        end
    end

    -- 有空位: 若本账户之前在排队, 此刻放行, 出队并更新平均放行间隔。
    -- 仅当排队开关开启 (cap>0) 时队列才可能非空; cap=0 (默认) 直接跳过, 省一次 LRANGE 往返。
    if cap > 0 and xEngine.LoginQueue.position(accountId) > 0 then
        xEngine.LoginQueue.pop()
        xEngine.LoginQueue.recordDequeue()
    end

    -- 一次读全量 session 字段 (state/last_seen/gate_process/cid), 区分「已进入游戏 → 顶号」
    -- 与「登录中 → 超时判定」, 替代逐个 getField 的多次 Redis 往返。
    local sess     = xEngine.AccountSession.getAll(accountId)
    local oldState = tonumber(sess.state) or xEngine.SessionState.OFFLINE
    local lastSeen = tonumber(sess.last_seen) or 0
    local now      = luacore.si_time_cur_sec()

    -- 登录中 (LOGGED_IN/ROLE_SELECTED): 未超时则拒绝新登录 (并发登录互相覆盖防护),
    -- 已超时视为陈旧登录, 放行并按顶号流程替换。
    if oldState == xEngine.SessionState.LOGGED_IN or oldState == xEngine.SessionState.ROLE_SELECTED then
        if now - lastSeen <= LOGIN_TIMEOUT_SEC then
            xEngine.AccountSession.releaseLock(accountId)
            xEngine.Log.warn("[Login] login: account=%d logging in (state=%d, %ds ago), reject",
                accountId, oldState, now - lastSeen)
            return luacore.si_pb_login_rsp(EC.SESSION_INVALID, 0, "")
        end
        xEngine.Log.info("[Login] login: account=%d stale login (state=%d, %ds ago), replace",
            accountId, oldState, now - lastSeen)
    end

    -- 已进入游戏 (IN_GAME/IN_BATTLE) 或陈旧登录: 顶号踢旧连接。先写 kicked 标记再异步踢旧,
    -- 旧连接的断线收尾据此跳过离线处理 (避免覆盖新 session); 新连接所在 Gate 回包后补写
    -- gate_process/cid 完成「更新 Gate 客户端信息」。
    local oldGateProcess = tonumber(sess.gate_process)
    local oldCid         = tonumber(sess.cid)
    local kickedOld = false
    if oldGateProcess and oldGateProcess > 0 and oldCid and oldCid > 0 then
        kickedOld = true
        xEngine.AccountSession.set(accountId, { kicked = 1 })
        xEngine.Log.info("[Login] login: account=%d kick old gate=%d cid=%d (state=%d)",
            accountId, oldGateProcess, oldCid, oldState)
        luacore.si_gate_kick_cid(oldGateProcess, oldCid)
    end

    -- 写新 session (gate_process/cid 由新连接所在 Gate 回包后补写)
    xEngine.AccountSession.set(accountId, {
        state         = xEngine.SessionState.LOGGED_IN,
        login_process = luacore.si_get_process_id(),
        token         = token,
        last_seen     = now,
        kicked        = kickedOld and 1 or 0,
    })
    xEngine.AccountSession.releaseLock(accountId)

    -- 账户状态机: 登录即进入 ROLE_LISTED (回包携带角色列表); 中间态 LOGGED_IN 无观察者,
    -- 折叠为单次写 (省 1 次 Redis Set)。
    xEngine.Account.setState(accountId, xEngine.AccountState.ROLE_LISTED)

    local username = "account_" .. xEngine.Number.intKey(accountId)
    local roles = xEngine.Role.listRoles(accountId)
    local rolesStr = xEngine.Role.encode(roles)
    xEngine.Log.info("[Login] login: account=%d session ok roles=%d", accountId, #roles)
    return luacore.si_pb_login_rsp_roles(EC.NONE, accountId, username, rolesStr)
end

-- 创角: 校验角色名 → 分配 roleId → 写入角色列表/索引 → 返回 CreateRoleRsp
function LoginClass:rpc_create_role(payload)
    local accountId = luacore.si_pb_create_role_account_id(payload)
    local roleName  = luacore.si_pb_create_role_role_name(payload)
    local job       = luacore.si_pb_create_role_job(payload)

    -- 角色名校验: 非空 / 长度 / 不含定界符与控制字符 (保证列表定界字符串可解析;
    -- | 与 : 用于 player:{roleId} 的 "name|level|job" 及快照定界, 一并排除)
    if not roleName or roleName == "" or #roleName > 32 or roleName:find("[,;|:%c]") then
        xEngine.Log.warn("[Login] create_role: invalid name account=%d", accountId)
        return luacore.si_pb_create_role_rsp(EC.ROLE_NAME_INVALID, 0, "", 0, 0)
    end

    -- 全局重名校验
    if xEngine.Role.nameTaken(roleName) then
        xEngine.Log.warn("[Login] create_role: name taken account=%d name=%s", accountId, roleName)
        return luacore.si_pb_create_role_rsp(EC.ROLE_NAME_TAKEN, 0, "", 0, 0)
    end

    -- 角色 id 分配: accountId*1000 + 序号 (同步、全局唯一, 免 Redis INCR)。
    -- 一次 listRoles 复用到底: 既算序号又传给 addRole 追加, 省 addRole 内二次 listRoles 往返。
    local roles  = xEngine.Role.listRoles(accountId)
    local roleId = accountId * 1000 + #roles + 1

    if not xEngine.Role.addRole(accountId, roleId, roleName, 1, job, nil, roles) then
        return luacore.si_pb_create_role_rsp(EC.INTERNAL, 0, "", 0, 0)
    end

    xEngine.Log.info("[Login] create_role: account=%d role=%d name=%s job=%d", accountId, roleId, roleName, job)
    return luacore.si_pb_create_role_rsp(EC.NONE, roleId, roleName, 1, job)
end

-- 选角: 校验角色归属 → 更新状态 → 负载均衡创建 Player Actor → 返回 SelectRoleRsp
function LoginClass:rpc_select_role(payload)
    local accountId = luacore.si_pb_select_role_account_id(payload)
    local roleId    = luacore.si_pb_select_role_role_id(payload)

    -- 校验角色归属 (不再信任客户端提交的 roleId)
    local role = xEngine.Role.belongsTo(accountId, roleId)
    if not role then
        xEngine.Log.warn("[Login] select_role: role=%d not belong to account=%d", roleId, accountId)
        return luacore.si_pb_select_role_rsp(EC.ROLE_NOT_FOUND, 0, roleId, "")
    end

    xEngine.Account.setState(accountId, xEngine.AccountState.ROLE_SELECTED)

    -- 排队兜底: 登录与选角之间可能被顶穿 (新 Player 挤满), 建 Player 前再查一次上限
    local cap = self:get_player_actor_cap()
    if cap > 0 then
        local count = tonumber(xEngine.Redis.Get("cluster:load:player_count")) or 0
        if count >= cap then
            xEngine.LoginQueue.enqueue(accountId)
            xEngine.Log.warn("[Login] select_role: account=%d re-queued (count=%d cap=%d)",
                accountId, count, cap)
            return luacore.si_pb_select_role_rsp(EC.QUEUED, 0, roleId, "")
        end
    end

    -- 选角完成 → 挑最低负载 Game 进程 → RPC 创建 Player Actor (产生 Login→Game 链路)
    -- 幂等性在 C++ 侧 RPC_METHOD_CREATE_ACTOR 保证: 若 Player 已存在则直接复用,
    -- 断线重连/重复选角不会因重复建 Actor 而失败。
    local created = xEngine.ActorCreator.balanceCreate(AT.PLAYER, roleId)
    if not created then
        xEngine.Log.error("[Login] select_role: create player actor failed role=%d", roleId)
        return luacore.si_pb_select_role_rsp(EC.NO_AVAILABLE_PROCESS, 0, roleId, "")
    end

    -- 上线: 移除下线列表成员 (断线入列、选角上线出列) + 通知 Player 恢复 online=1。
    local pidKey = xEngine.Number.intKey(roleId)
    xEngine.Redis.SRem("offline:players", pidKey)
    xEngine.Redis.Del("offline:ts:" .. pidKey)
    luacore.si_forward_rpc_async(AT.PLAYER, roleId,
        luacore.si_net_body_wrap(roleId, encode_small_type(PLAYER_ONLINE)))

    xEngine.Account.setState(accountId, xEngine.AccountState.IN_GAME)

    local playerId = roleId

    -- 断线重连: 选角时查玩家是否仍在战斗中。battle:byplayer:{pid} 为权威源 (room 开战写入、
    -- 战斗结束清理), 命中则读回战斗直连信息 (ip/port/key) 随选角响应下发, 客户端据此直连重连
    -- BattleActor; session 同步置 IN_BATTLE 供「查询 session 当前状态」用。
    local pidKey = xEngine.Number.intKey(roleId)
    local battleIdStr = xEngine.Redis.Get("battle:byplayer:" .. pidKey)
    local battleId = (battleIdStr and battleIdStr ~= "") and tonumber(battleIdStr) or 0
    if battleId > 0 then
        local battleHost = xEngine.Redis.Get("battle:host:" .. xEngine.Number.intKey(battleId)) or ""
        local battleAuth = xEngine.Redis.Get("battle:auth:" .. xEngine.Number.intKey(battleId)) or ""
        xEngine.AccountSession.set(accountId, {
            state       = xEngine.SessionState.IN_BATTLE,
            battle_id   = xEngine.Number.intKey(battleId),
            battle_host = battleHost,
            battle_auth = battleAuth,
            last_seen   = luacore.si_time_cur_sec(),
            kicked      = 0,   -- 顶号流程结束: 新连接完成选角, 清除踢旧标记
        })
        xEngine.Log.info("[Login] select_role: player=%d reconnect battle=%d host=%s",
            playerId, battleId, battleHost)
        return luacore.si_pb_select_role_rsp_battle(EC.NONE, playerId, roleId, role.name,
            battleId, battleHost, battleAuth)
    end

    -- 未在战斗中: 常规进大厅 (session 状态回 IN_GAME)
    xEngine.AccountSession.set(accountId, {
        state = xEngine.SessionState.IN_GAME,
        last_seen = luacore.si_time_cur_sec(),
        kicked = 0,   -- 顶号流程结束: 新连接完成选角, 清除踢旧标记
    })

    xEngine.Log.info("[Login] select_role: player=%d created on Game", playerId)
    return luacore.si_pb_select_role_rsp(EC.NONE, playerId, roleId, role.name)
end

xEngine.ActorMgr.registerPrototype(AT.LOGIN, LoginClass)
xEngine.ActorMgr.install(AT.LOGIN, 0)
