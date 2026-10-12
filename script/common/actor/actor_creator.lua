-- actor_creator.lua — Global actor creator with RPC routing
-- Registered as xEngine.ActorCreator.
--
-- Responsibilities (CROSS-PROCESS):
--   1. Global create/find/delete/query — auto-routes local or RPC (负载均衡)
--   2. Load balancer API — balanceCreate, getProcessLoad
--
-- Routing logic:
--   - If type is hosted locally → delegate to xEngine.ActorMgr
--   - If type is remote → 自建 asio-TCP RPC (si_actor_creator_rpc_*),
--     由 C++ 侧 CActorLoadManager 统一按 Actor 类型均衡负载挑最低承载进程
--
-- Usage:
--   require("common.actor.actor_creator")
--
--   -- Global operations
--   local actor = xEngine.ActorCreator.create(100, 12345)
--   local found = xEngine.ActorCreator.find(100, 12345)
--   xEngine.ActorCreator.delete(100, 12345)
--
--   -- Load balancer
--   local actor = xEngine.ActorCreator.balanceCreate(100, 12345)
--   local load  = xEngine.ActorCreator.getProcessLoad()

xEngine = xEngine or {}
xEngine.ActorCreator = {}

--------------------------------------------------------------------------------
-- Global operations: create / find / delete / query
--------------------------------------------------------------------------------

--- Create an actor instance globally.
-- If type is local → xEngine.ActorMgr.createLocal
-- If type is remote → 自建 RPC 负载均衡创建 (CActorLoadManager)
--
-- @param actorType  int32
-- @param actorID    int64
-- @return table | nil
function xEngine.ActorCreator.create(actorType, actorID)
    -- 本进程 host 此 type → 本地创建
    if xEngine.ActorMgr.canHostType(actorType) then
        return xEngine.ActorMgr.createLocal(actorType, actorID)
    end

    -- 远程 → 自建 RPC + 负载均衡 (C++ 挑最低负载承载进程)
    xEngine.Log.info("[ActorCreator] create: rpc balance type=%d id=%d", actorType, actorID)
    local ok = luacore.si_actor_creator_rpc_create(actorType, actorID)
    if ok then
        xEngine.Log.info("[ActorCreator] create: remote ok")
        return { _actor_type = actorType, _actor_id = actorID, _remote = true }
    end
    xEngine.Log.error("[ActorCreator] create: rpc failed type=%d id=%d", actorType, actorID)
    return nil
end

--- Find an actor globally.
-- Checks local first, then remote via 自建 RPC.
function xEngine.ActorCreator.find(actorType, actorID)
    -- Local first
    local localResult = xEngine.ActorMgr.findLocal(actorType, actorID)
    if localResult then return localResult end

    -- Remote query via 自建 RPC
    local found, remoteType = luacore.si_actor_creator_rpc_find(actorType, actorID)
    if found then
        return { _actor_type = remoteType, _actor_id = actorID, _remote = true }
    end
    return nil
end

--- Delete an actor globally.
function xEngine.ActorCreator.delete(actorType, actorID)
    -- 本进程 → 本地删除
    if xEngine.ActorMgr.findLocal(actorType, actorID) then
        return xEngine.ActorMgr.deleteLocal(actorType, actorID)
    end

    -- 远程 → 自建 RPC 卸载
    return luacore.si_actor_creator_rpc_uninstall(actorType, actorID)
end

--- Query all instances of a type (local only for now).
function xEngine.ActorCreator.query(actorType)
    return xEngine.ActorMgr.getByType(actorType)
end

--------------------------------------------------------------------------------
-- Load balancer API
--------------------------------------------------------------------------------

--- Get this process's current load score.
-- Called by load balancer to decide where to create new actors.
-- @return int32   Higher = more loaded
function xEngine.ActorCreator.getProcessLoad()
    local count = xEngine.ActorMgr.getInstanceCount()
    -- Simple load: instance count as load indicator
    -- Can be extended to include CPU/memory/network metrics
    return count
end

--- Create an actor on the least-loaded process.
-- 1. 本进程 host 此 type → 本地创建
-- 2. 否则 → 自建 RPC (C++ CActorLoadManager 挑最低负载承载进程)
--
-- @param actorType  int32
-- @param actorID    int64
-- @return table | nil   The created actor
function xEngine.ActorCreator.balanceCreate(actorType, actorID)
    if xEngine.ActorMgr.canHostType(actorType) then
        return xEngine.ActorMgr.createLocal(actorType, actorID)
    end

    local ok = luacore.si_actor_creator_rpc_create(actorType, actorID)
    if ok then
        xEngine.Log.info("[ActorCreator] balanceCreate: remote ok type=%d id=%d", actorType, actorID)
        return { _actor_type = actorType, _actor_id = actorID, _remote = true }
    end
    xEngine.Log.error("[ActorCreator] balanceCreate: rpc failed type=%d id=%d", actorType, actorID)
    return nil
end

return xEngine.ActorCreator
