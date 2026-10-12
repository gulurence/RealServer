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
    INTERNAL             = 1999,
}

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

-- 登录: 校验 Session → 更新账户状态 → 返回 LoginRsp
function LoginClass:rpc_login(payload)
    local token     = luacore.si_pb_login_req_token(payload)
    local accountId = xEngine.Session.validate(token)

    if not accountId then
        xEngine.Log.warn("[Login] login: invalid session token")
        return luacore.si_pb_login_rsp(EC.SESSION_INVALID, 0, "")
    end

    xEngine.Account.setState(accountId, xEngine.AccountState.LOGGED_IN)
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

    -- 角色 id 分配: accountId*1000 + 序号 (同步、全局唯一, 免 Redis INCR)
    local count  = #xEngine.Role.listRoles(accountId)
    local roleId = accountId * 1000 + count + 1

    if not xEngine.Role.addRole(accountId, roleId, roleName, 1, job) then
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

    -- 选角完成 → 挑最低负载 Game 进程 → RPC 创建 Player Actor (产生 Login→Game 链路)
    -- 幂等性在 C++ 侧 RPC_METHOD_CREATE_ACTOR 保证: 若 Player 已存在则直接复用,
    -- 断线重连/重复选角不会因重复建 Actor 而失败。
    local created = xEngine.ActorCreator.balanceCreate(AT.PLAYER, roleId)
    if not created then
        xEngine.Log.error("[Login] select_role: create player actor failed role=%d", roleId)
        return luacore.si_pb_select_role_rsp(EC.NO_AVAILABLE_PROCESS, 0, roleId, "")
    end

    xEngine.Account.setState(accountId, xEngine.AccountState.IN_GAME)

    local playerId = roleId
    xEngine.Log.info("[Login] select_role: player=%d created on Game", playerId)
    return luacore.si_pb_select_role_rsp(EC.NONE, playerId, roleId, role.name)
end

xEngine.ActorMgr.registerPrototype(AT.LOGIN, LoginClass)
xEngine.ActorMgr.install(AT.LOGIN, 0)
