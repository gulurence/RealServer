-- actor_mgr.lua — Local actor instance manager
-- Registered as xEngine.ActorMgr.
--
-- Responsibilities (ALL LOCAL, per-process):
--   1. Template management — registerPrototype(type, classTable)
--   2. Local instance lifecycle — createLocal / findLocal / deleteLocal
--   3. Environment management — install / uninstall (with connection ref counting)
--   4. Process hosting — hostType / canHostType
--
-- For cross-process operations, use xEngine.ActorCreator.
--
-- Usage:
--   require("common.actor.actor_mgr")
--
--   -- Process startup
--   xEngine.ActorMgr.hostType(100)
--   local MyClass = xEngine.Class.define("MyActor", xEngine.ActorBase)
--   xEngine.ActorMgr.registerPrototype(100, MyClass)
--   xEngine.ActorMgr.install(100, 0)
--
--   -- Runtime (local)
--   local actor = xEngine.ActorMgr.createLocal(100, 12345)
--   local found = xEngine.ActorMgr.findLocal(100, 12345)
--   xEngine.ActorMgr.deleteLocal(100, 12345)

xEngine = xEngine or {}
xEngine.ActorMgr = {}

-- Lua 5.4 numbers are floats by default; SWIG expects integer subtype.
-- Use luacore.toint32/toint64 to explicitly cast before C++ calls.
local i32 = luacore.toint32
local i64 = luacore.toint64

--------------------------------------------------------------------------------
-- Internal state
--------------------------------------------------------------------------------
local protoRegistry = {}       -- type → { classTable, instance, cppActor, className }
local localInstances = {}      -- type → { id → Lua instance }
local hostedTypes = {}         -- { [type] = true }

--------------------------------------------------------------------------------
-- Process hosting
--------------------------------------------------------------------------------

function xEngine.ActorMgr.hostType(actorType)
    if hostedTypes[actorType] then return end
    hostedTypes[actorType] = true
    luacore.si_actor_mgr_add_hosted_type(i32(actorType))
    xEngine.Log.info("[ActorMgr] Host type declared: %d", actorType)
end

function xEngine.ActorMgr.canHostType(actorType)
    return hostedTypes[actorType] == true
end

function xEngine.ActorMgr.getHostedTypes()
    local result = {}
    for t, _ in pairs(hostedTypes) do result[t] = true end
    return result
end

--------------------------------------------------------------------------------
-- Template (prototype) management
--------------------------------------------------------------------------------

function xEngine.ActorMgr.registerPrototype(actorType, classTable)
    if not classTable then
        xEngine.Log.error("[ActorMgr] registerPrototype: classTable nil type=%d", actorType)
        return false
    end

    if protoRegistry[actorType] then
        xEngine.Log.warn("[ActorMgr] registerPrototype: type=%d updating", actorType)
    end

    if not hostedTypes[actorType] then
        xEngine.ActorMgr.hostType(actorType)
    end

    local className = classTable._className or ("ActorType_" .. tostring(actorType))
    local protoInstance = classTable:new(actorType, 0, className)
    local cppProto = luacore.si_lua_actor_create(i32(actorType), 0, className)

    if not cppProto then
        xEngine.Log.error("[ActorMgr] C++ CLuaActor create failed type=%d", actorType)
        return false
    end

    if not luacore.si_actor_regist(i32(actorType), cppProto) then
        xEngine.Log.error("[ActorMgr] si_actor_regist failed type=%d", actorType)
        return false
    end

    protoRegistry[actorType] = {
        classTable = classTable,
        instance   = protoInstance,
        cppActor   = cppProto,
        className  = className,
    }

    xEngine.Log.info("[ActorMgr] Prototype registered: type=%d class=%s", actorType, className)
    return true
end

function xEngine.ActorMgr.getPrototype(actorType)
    return protoRegistry[actorType]
end

--------------------------------------------------------------------------------
-- Environment: install / uninstall (connection ref counting)
--------------------------------------------------------------------------------

function xEngine.ActorMgr.install(actorType, actorID)
    local proto = protoRegistry[actorType]
    if not proto then
        xEngine.Log.error("[ActorMgr] install: type=%d not registered", actorType)
        return false
    end

    local ok = luacore.si_actor_mgr_install(i32(actorType), i64(actorID))
    if not ok then
        xEngine.Log.error("[ActorMgr] install: C++ failed type=%d id=%d", actorType, actorID)
        return false
    end

    local instance = proto.instance
    if actorID ~= 0 then
        instance = localInstances[actorType] and localInstances[actorType][actorID]
    end
    if instance and type(instance.onInstall) == "function" then
        instance:onInstall()
    end

    xEngine.Log.info("[ActorMgr] Installed type=%d id=%d", actorType, actorID)
    return true
end

function xEngine.ActorMgr.uninstall(actorType, actorID)
    local instance = localInstances[actorType] and localInstances[actorType][actorID]

    if instance and type(instance.onUnInstall) == "function" then
        instance:onUnInstall()
    end

    local ok = luacore.si_actor_mgr_uninstall(i32(actorType), i64(actorID))

    if localInstances[actorType] then
        localInstances[actorType][actorID] = nil
        if next(localInstances[actorType]) == nil then
            localInstances[actorType] = nil
        end
    end

    xEngine.Log.info("[ActorMgr] Uninstalled type=%d id=%d ok=%s", actorType, actorID, tostring(ok))
    return ok
end

function xEngine.ActorMgr.uninstallType(actorType)
    local typeInstances = localInstances[actorType]
    if typeInstances then
        for id, _ in pairs(typeInstances) do
            xEngine.ActorMgr.uninstall(actorType, id)
        end
    end

    luacore.si_actor_mgr_uninstall(i32(actorType), 0)
    xEngine.Log.info("[ActorMgr] Uninstalled all type=%d", actorType)
    return true
end

--------------------------------------------------------------------------------
-- Local instance lifecycle
--------------------------------------------------------------------------------

function xEngine.ActorMgr.createLocal(actorType, actorID)
    if not hostedTypes[actorType] then
        xEngine.Log.error("[ActorMgr] createLocal: type=%d not hosted locally", actorType)
        return nil
    end

    local existing = xEngine.ActorMgr.findLocal(actorType, actorID)
    if existing then return existing end

    local proto = protoRegistry[actorType]
    if not proto then
        xEngine.Log.error("[ActorMgr] createLocal: no prototype type=%d", actorType)
        return nil
    end

    local instanceName = proto.className .. "_" .. tostring(actorID)
    local luaInstance = proto.classTable:new(actorType, actorID, instanceName)
    if not luaInstance then return nil end

    local cppActor = luacore.si_actor_create(i32(actorType), i64(actorID))
    if not cppActor then return nil end

    luaInstance._cpp_actor = cppActor
    luaInstance._lua_ref = luaInstance

    -- Install environment
    xEngine.ActorMgr.install(actorType, actorID)

    -- Load data
    if type(luaInstance.data_load) == "function" then
        luaInstance:data_load("")
    end

    -- Init
    if type(luaInstance.onInit) == "function" then
        if not luaInstance:onInit() then return nil end
    end

    luacore.si_actor_mgr_add_instance(cppActor)

    if not localInstances[actorType] then
        localInstances[actorType] = {}
    end
    localInstances[actorType][actorID] = luaInstance

    xEngine.Log.info("[ActorMgr] Local create: type=%d id=%d", actorType, actorID)
    return luaInstance
end

function xEngine.ActorMgr.findLocal(actorType, actorID)
    local typeInstances = localInstances[actorType]
    if typeInstances and typeInstances[actorID] then
        return typeInstances[actorID]
    end
    return nil
end

function xEngine.ActorMgr.deleteLocal(actorType, actorID)
    local instance = localInstances[actorType] and localInstances[actorType][actorID]
    if not instance then
        xEngine.Log.warn("[ActorMgr] deleteLocal: not found type=%d id=%d", actorType, actorID)
        return false
    end

    -- Save before delete
    if type(instance.data_save) == "function" then
        instance:data_save(3)  -- save_type=3 (delete)
    end

    -- Uninstall
    xEngine.ActorMgr.uninstall(actorType, actorID)

    -- Remove C++ actor
    luacore.si_actor_creator_delete(i32(actorType), i64(actorID))

    xEngine.Log.info("[ActorMgr] Local delete: type=%d id=%d", actorType, actorID)
    return true
end

function xEngine.ActorMgr.getByType(actorType)
    return localInstances[actorType] or {}
end

function xEngine.ActorMgr.getInstanceCount()
    return luacore.si_actor_mgr_get_instance_count()
end

return xEngine.ActorMgr
