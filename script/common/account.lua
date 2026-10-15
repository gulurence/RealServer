-- account.lua — 账号 / 会话 / 角色状态 (Redis 热数据 + MySQL 兜底)
-- 注册为 xEngine.Account / xEngine.Session / xEngine.AccountState。
--
-- 数据键约定 (与 WebPlatform 的 config.php 及 RedisCluster 一致):
--   account:{id}          → 账号信息 (Redis string, 热数据)
--   account:{id}:state    → 账户状态机 (0=Created 1=LoggedIn 2=RoleListed 3=RoleSelected 4=InGame)
--   session:{token}       → 账号 id (Redis string, WebPlatform 签发 + TTL)
--   player:{id}           → 玩家/角色信息 (Redis string, 选角后热数据)
--
-- 账户状态机在需求里反复出现「更新游戏账户状态」, 每次流转都写回 Redis。
--
-- Usage:
--   require("common.account")
--   xEngine.Account.setState(id, xEngine.AccountState.LOGGED_IN)
--   local accountId = xEngine.Session.validate(token)
--   local roleData  = xEngine.Role.load(roleId)

xEngine = xEngine or {}

--------------------------------------------------------------------------------
-- 账户状态机 (镜像 source/proto-def/msg/msg_id.proto 的 AccountState 枚举)
--------------------------------------------------------------------------------
xEngine.AccountState = {
    CREATED       = 0,   -- 账户已创建
    LOGGED_IN     = 1,   -- 已登录 (Session 校验通过)
    ROLE_LISTED   = 2,   -- 已拉取角色列表
    ROLE_SELECTED = 3,   -- 已选角
    IN_GAME       = 4,   -- 已进入游戏 (player actor 创建完成)
}

--------------------------------------------------------------------------------
-- Account — 账号状态 + 热数据
--------------------------------------------------------------------------------
xEngine.Account = {}

function xEngine.Account.stateKey(id)
    return "account:" .. xEngine.Number.intKey(id) .. ":state"
end

function xEngine.Account.infoKey(id)
    return "account:" .. xEngine.Number.intKey(id)
end

--- 读取账户状态
-- @param id int64
-- @return int32  状态值 (缺省 0=CREATED)
function xEngine.Account.getState(id)
    local v = xEngine.Redis.Get(xEngine.Account.stateKey(id))
    if v == nil or v == "" then
        return xEngine.AccountState.CREATED
    end
    return tonumber(v) or xEngine.AccountState.CREATED
end

--- 更新账户状态 (写回 Redis, 同步)
-- @param id     int64
-- @param state  int32  xEngine.AccountState.*
-- @return bool
function xEngine.Account.setState(id, state)
    local ok = xEngine.Redis.Set(xEngine.Account.stateKey(id), tostring(state))
    xEngine.Log.info("[Account] setState account=%d state=%d ok=%s", id, state, tostring(ok))
    return ok
end

--- 加载账号信息 (Redis 命中优先, miss 回 MySQL 再写 Redis)
-- @param id int64
-- @return string | nil
function xEngine.Account.load(id)
    local key = xEngine.Account.infoKey(id)
    local cached = xEngine.Redis.Get(key)
    if cached and cached ~= "" then
        return cached
    end

    -- DB 兜底 (异步; 结果经 operator 回调, 此处仅触发并缓存占位)
    local dbData = xEngine.DB.Select("account", "id=" .. xEngine.Number.intKey(id))
    if dbData and dbData ~= "" then
        xEngine.Redis.Set(key, dbData)
        return dbData
    end
    return nil
end

--- 保存账号信息 (先写 Redis, 再异步落库)
-- @param id    int64
-- @param data  string  序列化后的账号信息
-- @return bool
function xEngine.Account.save(id, data)
    local ok = xEngine.Redis.Set(xEngine.Account.infoKey(id), data)
    xEngine.DB.Insert("account", data)
    return ok
end

--------------------------------------------------------------------------------
-- Session — 会话令牌校验 (WebPlatform 签发, Lua 侧校验)
--------------------------------------------------------------------------------
xEngine.Session = {}

function xEngine.Session.key(token)
    return "session:" .. tostring(token)
end

--- 创建会话 (测试/回退用; 生产由 WebPlatform 签发并设 TTL)
-- @param token      string
-- @param accountId  int64
-- @return bool
function xEngine.Session.create(token, accountId)
    return xEngine.Redis.Set(xEngine.Session.key(token), xEngine.Number.intKey(accountId))
end

--- 校验会话令牌 → 返回账号 id
-- @param token string
-- @return int64 | nil  会话不存在/过期返回 nil
function xEngine.Session.validate(token)
    if not token or token == "" then
        return nil
    end
    local v = xEngine.Redis.Get(xEngine.Session.key(token))
    if v == nil or v == "" then
        return nil
    end
    return xEngine.Number.toInt(tonumber(v))
end

--- 销毁会话
-- @param token string
-- @return bool
function xEngine.Session.destroy(token)
    return xEngine.Redis.Del(xEngine.Session.key(token))
end

--------------------------------------------------------------------------------
-- Role / Player — 角色列表 + 选角后热数据
--------------------------------------------------------------------------------
xEngine.Role = {}

function xEngine.Role.key(roleId)
    return "player:" .. xEngine.Number.intKey(roleId)
end

function xEngine.Role.load(roleId)
    local v = xEngine.Redis.Get(xEngine.Role.key(roleId))
    if v == nil or v == "" then
        return nil
    end
    return v
end

function xEngine.Role.save(roleId, data)
    return xEngine.Redis.Set(xEngine.Role.key(roleId), data)
end

--------------------------------------------------------------------------------
-- 角色属性 (基础 = 昵称/等级/当前经验/金币; 战斗 = 血量/攻击/防御)
--   player:{roleId} 记录从旧 3 字段 "name|level|job" 扩展为 8 字段
--   "name|level|job|exp|gold|attack|defense|hp"。旧记录缺省回退
--   (exp=0, gold=0, attack=10, defense=0, hp=100)。战斗房间带入属性与
--   结算回传共用这一套 parse/encode/update。
--------------------------------------------------------------------------------
function xEngine.Role.parseAttrs(roleId)
    local raw = xEngine.Role.load(roleId)
    local name, level, job, exp, gold, attack, defense, hp = "", 1, 0, 0, 0, 10, 0, 100
    if raw and raw ~= "" then
        name, level, job, exp, gold, attack, defense, hp =
            raw:match("^(.-)|(.-)|(.-)|(.-)|(.-)|(.-)|(.-)|(.-)$")
        if not name then
            -- 兼容旧 3 字段记录
            name, level, job = raw:match("^(.-)|(.-)|(.-)$")
            exp, gold, attack, defense, hp = 0, 0, 10, 0, 100
        end
    end
    return {
        name    = name or "",
        level   = tonumber(level) or 1,
        job     = tonumber(job) or 0,
        exp     = tonumber(exp) or 0,
        gold    = tonumber(gold) or 0,
        attack  = tonumber(attack) or 10,
        defense = tonumber(defense) or 0,
        hp      = tonumber(hp) or 100,
    }
end

function xEngine.Role.encodeAttrs(attrs)
    return string.format("%s|%d|%d|%d|%d|%d|%d|%d",
        attrs.name or "", attrs.level or 1, attrs.job or 0,
        attrs.exp or 0, attrs.gold or 0, attrs.attack or 10,
        attrs.defense or 0, attrs.hp or 100)
end

function xEngine.Role.saveAttrs(roleId, attrs)
    return xEngine.Redis.Set(xEngine.Role.key(roleId), xEngine.Role.encodeAttrs(attrs))
end

--- 应用数值增量 (exp/gold/attack/defense/hp 可为负), gold/hp 钳位 ≥0; 写回 Redis。
-- @return 更新后的属性表
function xEngine.Role.updateAttrs(roleId, delta)
    local a = xEngine.Role.parseAttrs(roleId)
    if delta.exp      then a.exp      = a.exp + delta.exp end
    if delta.gold     then a.gold     = math.max(0, a.gold + delta.gold) end
    if delta.attack   then a.attack   = a.attack + delta.attack end
    if delta.defense  then a.defense  = a.defense + delta.defense end
    if delta.hp       then a.hp       = math.max(0, a.hp + delta.hp) end
    xEngine.Role.saveAttrs(roleId, a)
    return a
end

--------------------------------------------------------------------------------
-- Role 集合 / 重名索引 (同步热数据, 无 JSON — 定界字符串)
--   account:{id}:roles  → "roleId,roleName,level,job;roleId,roleName,level,job;..."
--   rolename:{name}     → role_id (全局重名索引, name 已转小写)
--------------------------------------------------------------------------------
function xEngine.Role.listKey(accountId)
    return "account:" .. xEngine.Number.intKey(accountId) .. ":roles"
end

function xEngine.Role.nameKey(name)
    return "rolename:" .. string.lower(tostring(name))
end

--- 拉取账户角色列表 (同步)
-- @return list<{roleId=int, name=string, level=int, job=int}>
function xEngine.Role.listRoles(accountId)
    local roles = {}
    local raw = xEngine.Redis.Get(xEngine.Role.listKey(accountId))
    if not raw or raw == "" then
        return roles
    end
    for item in raw:gmatch("[^;]+") do
        local roleId, name, level, job = item:match("^(.-),(.-),(.-),(.-)$")
        if roleId and name then
            roles[#roles + 1] = {
                roleId = xEngine.Number.toInt(tonumber(roleId)),
                name   = name,
                level  = tonumber(level) or 1,
                job    = tonumber(job) or 0,
            }
        end
    end
    return roles
end

--- 角色名是否已被占用 (全局)
function xEngine.Role.nameTaken(name)
    local v = xEngine.Redis.Get(xEngine.Role.nameKey(name))
    return v ~= nil and v ~= ""
end

--- 序列化角色列表 → 定界字符串 (供 si_pb_login_rsp_roles / Redis 存储)
function xEngine.Role.encode(roles)
    local parts = {}
    for _, r in ipairs(roles) do
        parts[#parts + 1] = string.format("%d,%s,%d,%d", r.roleId, r.name, r.level or 1, r.job or 0)
    end
    return table.concat(parts, ";")
end

--- 追加角色: 写列表 + player:{roleId} 数据 + 重名索引
-- @return bool
function xEngine.Role.addRole(accountId, roleId, name, level, job, data)
    local roles = xEngine.Role.listRoles(accountId)
    roles[#roles + 1] = { roleId = roleId, name = name, level = level or 1, job = job or 0 }
    local ok = xEngine.Redis.Set(xEngine.Role.listKey(accountId), xEngine.Role.encode(roles))
    xEngine.Redis.Set(xEngine.Role.nameKey(name), xEngine.Number.intKey(roleId))
    xEngine.Redis.Set(xEngine.Role.key(roleId), data or string.format("%s|%d|%d", name, level or 1, job or 0))
    return ok
end

--- 角色是否属于某账户; 命中返回角色条目, 否则 nil
function xEngine.Role.belongsTo(accountId, roleId)
    for _, r in ipairs(xEngine.Role.listRoles(accountId)) do
        if r.roleId == roleId then
            return r
        end
    end
    return nil
end

--------------------------------------------------------------------------------
-- 账户 session 同步 (跨 LoginActor 并发登录控制)
--   account:{id}:session → Hash, 记录该账户「当前整体游戏状态」登录→离线:
--     state        0=OFFLINE 1=LOGGED_IN 2=ROLE_SELECTED 3=IN_GAME 4=IN_BATTLE
--     login_process 当前登录所在 Login 进程 ID
--     gate_process  当前连接所在 Gate 进程 ID (踢旧时定向断开用)
--     cid           当前连接在 Gate 上的 CID
--     token / last_seen / kicked
--   整体设 TTL (30s), 心跳/消息续期; 进程宕机后由 Redis 过期兜底清理。
--------------------------------------------------------------------------------
xEngine.SessionState = {
    OFFLINE       = 0,
    LOGGED_IN     = 1,
    ROLE_SELECTED = 2,
    IN_GAME       = 3,
    IN_BATTLE     = 4,
}

xEngine.AccountSession = {}

function xEngine.AccountSession.key(accountId)
    return "account:" .. xEngine.Number.intKey(accountId) .. ":session"
end

function xEngine.AccountSession.lockKey(accountId)
    return "account:" .. xEngine.Number.intKey(accountId) .. ":login-lock"
end

--- 读 session 单字段 (不存在/空返回 nil)
function xEngine.AccountSession.getField(accountId, field)
    local v = xEngine.Redis.HGet(xEngine.AccountSession.key(accountId), field)
    if v == nil or v == "" then return nil end
    return v
end

--- 读 session 状态 (缺省 OFFLINE)
function xEngine.AccountSession.getState(accountId)
    return tonumber(xEngine.AccountSession.getField(accountId, "state"))
        or xEngine.SessionState.OFFLINE
end

--- 写 session 字段 (fields: field→value, 内部 tostring; 每次写刷新 TTL)
function xEngine.AccountSession.set(accountId, fields)
    local key = xEngine.AccountSession.key(accountId)
    for k, v in pairs(fields) do
        xEngine.Redis.HSet(key, k, tostring(v))
    end
    xEngine.Redis.Expire(key, 30)
end

--- 每账号登录短锁 (SET NX EX), 串行化并发登录; 成功返回 true
function xEngine.AccountSession.acquireLock(accountId, ttlSec)
    return xEngine.Redis.SetNxEx(xEngine.AccountSession.lockKey(accountId), "1", ttlSec or 5)
end

function xEngine.AccountSession.releaseLock(accountId)
    return xEngine.Redis.Del(xEngine.AccountSession.lockKey(accountId))
end

--------------------------------------------------------------------------------
-- 登录排队 (集群 PlayerActor 满载时的软限流)
--   queue:login                 → List, FIFO 存 accountId
--   queue:login:meta:{accountId} → Hash (enqueue_ts), 幂等入队标记
--   queue:login:avg_interval    → 平均放行间隔 (秒), 由每次出队滚动更新
--   queue:login:last_dequeue_ts → 上次出队时间戳 (算间隔用)
--------------------------------------------------------------------------------
xEngine.LoginQueue = {}

local QUEUE_KEY      = "queue:login"
local QUEUE_META     = "queue:login:meta:"
local QUEUE_AVG      = "queue:login:avg_interval"
local QUEUE_LAST_OUT = "queue:login:last_dequeue_ts"

function xEngine.LoginQueue.metaKey(accountId)
    return QUEUE_META .. xEngine.Number.intKey(accountId)
end

--- 幂等入队 (已在队则跳过)
function xEngine.LoginQueue.enqueue(accountId)
    local metaKey = xEngine.LoginQueue.metaKey(accountId)
    if xEngine.Redis.HGet(metaKey, "enqueue_ts") ~= "" then
        return
    end
    xEngine.Redis.HSet(metaKey, "enqueue_ts", tostring(luacore.si_time_cur_sec()))
    xEngine.Redis.RPush(QUEUE_KEY, xEngine.Number.intKey(accountId))
end

--- 队列位置 (1 起; 0 = 不在队中)
function xEngine.LoginQueue.position(accountId)
    return xEngine.Redis.LPos(QUEUE_KEY, xEngine.Number.intKey(accountId))
end

--- 队头出队并清理 meta; 空队返回 nil
function xEngine.LoginQueue.pop()
    local v = xEngine.Redis.LPop(QUEUE_KEY)
    if v and v ~= "" then
        xEngine.Redis.Del(QUEUE_META .. v)
        return tonumber(v)
    end
    return nil
end

--- 移除指定账户 (未必在队头; 幂等删除 meta, 但 Redis List 的按值删除代价高,
--  故仅在「该账户本就在队头」场景由 pop 出队; 此处仅清理 meta 供兜底)
function xEngine.LoginQueue.removeMeta(accountId)
    return xEngine.Redis.Del(xEngine.LoginQueue.metaKey(accountId))
end

--- 预计等待秒 = 位置 × 平均放行间隔 (平均等待时间)
function xEngine.LoginQueue.estWait(position)
    local avg = tonumber(xEngine.Redis.Get(QUEUE_AVG)) or 1
    return math.max(0, position * avg)
end

--- 出队放行时更新滚动平均间隔 (interval = 距上次放行的秒数)
function xEngine.LoginQueue.recordDequeue()
    local now = luacore.si_time_cur_sec()
    local last = tonumber(xEngine.Redis.Get(QUEUE_LAST_OUT))
    if last and last > 0 then
        local interval = now - last
        if interval > 0 then
            local avg = tonumber(xEngine.Redis.Get(QUEUE_AVG)) or 1
            avg = avg * 0.9 + interval * 0.1
            xEngine.Redis.Set(QUEUE_AVG, tostring(avg))
        end
    end
    xEngine.Redis.Set(QUEUE_LAST_OUT, tostring(now))
end
