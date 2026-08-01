-- actor_creator.lua — Global actor creator with RPC routing
-- Registered as xEngine.ActorCreator.
--
-- Responsibilities (CROSS-PROCESS):
--   1. Global create/find/delete/query — auto-routes local or gRPC
--   2. Load balancer API — balanceCreate, getProcessLoad
--   3. Route configuration — setRoute(type, channel)
--
-- Routing logic:
--   - If type is hosted locally → delegate to xEngine.ActorMgr
--   - If type is remote → gRPC forward to target process
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
-- Internal state
--------------------------------------------------------------------------------

-- type → { channel = "process_name", hosts = {"p1","p2"} }
local typeRoutes = {}

-- 本进程名称 (从配置注入)
local processName = ""

--------------------------------------------------------------------------------
-- Route configuration
--------------------------------------------------------------------------------

function xEngine.ActorCreator.setProcessName(name)
    processName = name
    xEngine.Log.info("[ActorCreator] Process name: %s", name)
end

function xEngine.ActorCreator.getProcessName()
    return processName
end

--- Configure routing for an actor type.
-- @param actorType  int32
-- @param channel    string   gRPC channel name for the target process
function xEngine.ActorCreator.setRoute(actorType, channel)
    typeRoutes[actorType] = { channel = channel }
    xEngine.Log.info("[ActorCreator] Route: type=%d → channel=%s", actorType, channel)
end

--- Get the route for a type.
function xEngine.ActorCreator.getTypeRoute(actorType)
    return typeRoutes[actorType]
end

--------------------------------------------------------------------------------
-- Global operations: create / find / delete / query
--------------------------------------------------------------------------------

--- Create an actor instance globally.
-- If type is local → xEngine.ActorMgr.createLocal
-- If type is remote → gRPC forward
-- If targetProcess is specified → route to that process
--
-- @param actorType      int32
-- @param actorID        int64
-- @param targetProcess  string (optional)  Force creation on a specific process
-- @return table | nil
function xEngine.ActorCreator.create(actorType, actorID, targetProcess)
    -- 1. 指定了目标进程 → 远程 gRPC 转发
    if targetProcess and targetProcess ~= processName then
        xEngine.Log.info("[ActorCreator] create: routing to process=%s type=%d id=%d",
            targetProcess, actorType, actorID)
        local ok = luacore.si_actor_creator_grpc_create(targetProcess, actorType, actorID)
        if ok then xEngine.Log.info("[ActorCreator] create: remote ok") end
        return ok
    end

    -- 2. 本进程 host 此 type → 本地创建
    if xEngine.ActorMgr.canHostType(actorType) then
        return xEngine.ActorMgr.createLocal(actorType, actorID)
    end

    -- 3. 查路由表 → gRPC 转发
    local route = typeRoutes[actorType]
    if route and route.channel then
        xEngine.Log.info("[ActorCreator] create: routing to channel=%s type=%d id=%d",
            route.channel, actorType, actorID)
        local ok = luacore.si_actor_creator_grpc_create(route.channel, actorType, actorID)
        if ok then xEngine.Log.info("[ActorCreator] create: remote ok") end
        return ok
    end

    xEngine.Log.error("[ActorCreator] create: no route for type=%d", actorType)
    return nil
end

--- Find an actor globally.
-- Checks local first, then remote via gRPC.
function xEngine.ActorCreator.find(actorType, actorID)
    -- Local first
    local localResult = xEngine.ActorMgr.findLocal(actorType, actorID)
    if localResult then return localResult end

    -- Remote query via gRPC
    local route = typeRoutes[actorType]
    if route and route.channel then
        xEngine.Log.debug("[ActorCreator] find: querying remote channel=%s", route.channel)
        local found, remoteType = luacore.si_actor_creator_grpc_find(route.channel, actorType, actorID)
        if found then
            return { _actor_type = remoteType, _actor_id = actorID, _remote = true }
        end
    end

    return nil
end

--- Delete an actor globally.
function xEngine.ActorCreator.delete(actorType, actorID, targetProcess)
    -- 1. 指定目标进程 → gRPC 远程转发
    if targetProcess and targetProcess ~= processName then
        xEngine.Log.info("[ActorCreator] delete: routing to process=%s", targetProcess)
        return luacore.si_actor_creator_grpc_uninstall(targetProcess, actorType, actorID)
    end

    -- 2. 本进程 → 本地删除
    if xEngine.ActorMgr.findLocal(actorType, actorID) then
        return xEngine.ActorMgr.deleteLocal(actorType, actorID)
    end

    -- 3. 远程 gRPC 删除
    local route = typeRoutes[actorType]
    if route and route.channel then
        return luacore.si_actor_creator_grpc_uninstall(route.channel, actorType, actorID)
    end

    xEngine.Log.warn("[ActorCreator] delete: not found type=%d id=%d", actorType, actorID)
    return false
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
-- 1. Query load from all processes that host this type
-- 2. Pick the one with lowest load
-- 3. gRPC create to that process
--
-- @param actorType  int32
-- @param actorID    int64
-- @return table | nil   The created actor
function xEngine.ActorCreator.balanceCreate(actorType, actorID)
    -- If we host this type locally, create here
    if xEngine.ActorMgr.canHostType(actorType) then
        return xEngine.ActorMgr.createLocal(actorType, actorID)
    end

    -- Remote: forward to the configured channel for this type.
    -- (Full load-balancing across multiple hosts requires a dedicated
    --  load-balancer service that queries all processes.)
    local route = typeRoutes[actorType]
    if route and route.channel then
        xEngine.Log.info("[ActorCreator] balanceCreate: routing to channel=%s type=%d id=%d",
            route.channel, actorType, actorID)
        local ok = luacore.si_actor_creator_grpc_create(route.channel, actorType, actorID)
        if ok then
            xEngine.Log.info("[ActorCreator] balanceCreate: remote ok")
            return { _actor_type = actorType, _actor_id = actorID, _remote = true }
        end
        return nil
    end

    xEngine.Log.error("[ActorCreator] balanceCreate: no route for type=%d", actorType)
    return nil
end

return xEngine.ActorCreator
