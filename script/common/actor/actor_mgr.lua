--[[
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: actor_mgr.lua
      Purpose:   进程内本地 Actor 实例管理器

      xEngine.ActorMgr 管理原型注册（registerPrototype）、本地实例生命周期（createLocal/findLocal/deleteLocal）与 install/uninstall（引用计数）。
      维护 protoRegistry/localInstances/protoInstalled，支持热更原地刷新原型；canHostType 由 C++ 托管类型注册决定。
]]

-- actor_mgr.lua — Local actor instance manager
-- Registered as xEngine.ActorMgr.
--
-- Responsibilities (ALL LOCAL, per-process):
--   1. Template management — registerPrototype(type, classTable)
--   2. Local instance lifecycle — createLocal / findLocal / deleteLocal
--   3. Environment management — install / uninstall (with connection ref counting)
--   4. Process hosting — canHostType (托管类型由 C++ CActorLoadManager 注册)
--
-- For cross-process operations, use xEngine.ActorCreator.
--
-- Usage:
--   require("common.actor.actor_mgr")
--
--   -- Process startup
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
local protoInstalled = {}      -- type → true (原型 id=0 已 Install, 热更幂等短路)

--------------------------------------------------------------------------------
-- Process hosting (托管类型由 C++ CActorLoadManager 在启动时从进程类型注册)
--------------------------------------------------------------------------------

function xEngine.ActorMgr.canHostType(actorType)
    return luacore.si_actor_mgr_can_host_type(i32(actorType))
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
        -- 热更: class.lua 已把 classTable 原地更新 (同一张表), 既有 protoInstance 经
        -- metatable.__index 指向该表自动获得新方法; C++ 侧 CLuaActor/delegate 无需重建
        -- (重建会泄漏原型对象并重复 si_actor_regist)。仅刷新 classTable/className 引用。
        local proto = protoRegistry[actorType]
        proto.classTable = classTable
        proto.className = classTable._className or proto.className
        xEngine.Log.info("[ActorMgr] Prototype hot-updated (in-place): type=%d class=%s",
            actorType, proto.className)
        return true
    end

    local className = classTable._className or ("ActorType_" .. xEngine.Number.intKey(actorType))
    local protoInstance = classTable:new(actorType, 0, className)

    -- 绑定 Lua 业务委托: si_lua_actor_create 会读取 _G.__actor_delegate,
    -- luaL_ref 后 SetLuaDelegate, 使 CLuaActor::OnMessage → on_msg 可达。
    _G.__actor_delegate = protoInstance
    local cppProto = luacore.si_lua_actor_create(i32(actorType), 0, className)
    _G.__actor_delegate = nil

    if not cppProto then
        xEngine.Log.error("[ActorMgr] C++ CLuaActor create failed type=%d", actorType)
        return false
    end

    if not luacore.si_actor_regist(i32(actorType), cppProto) then
        xEngine.Log.error("[ActorMgr] si_actor_regist failed type=%d", actorType)
        return false
    end

    -- Add prototype to ActorMgr so Install can find it
    if not luacore.si_actor_mgr_add_instance(cppProto) then
        xEngine.Log.error("[ActorMgr] add_instance (proto) failed type=%d", actorType)
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

    -- 幂等 (仅原型 id=0): 热更时各 actor init.lua 会再次 install(type, 0), 此处短路,
    -- 避免 C++ Install 重复执行 (AcquireServices 引用计数 +1 / actor->Install 二次回调 /
    -- RegisterActor 重复写), 也不重跑 onInstall。实例 (id≠0) 走 createLocal 每次新建,
    -- 不在热更路径, 保持原样 (deleteLocal 后同名重建需重新 Install)。
    if actorID == 0 and protoInstalled[actorType] then
        xEngine.Log.info("[ActorMgr] install: skip (proto already installed) type=%d id=%d", actorType, actorID)
        return true
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

    if actorID == 0 then
        protoInstalled[actorType] = true
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
    if not xEngine.ActorMgr.canHostType(actorType) then
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

    local instanceName = proto.className .. "_" .. xEngine.Number.intKey(actorID)
    local luaInstance = proto.classTable:new(actorType, actorID, instanceName)
    if not luaInstance then return nil end

    local cppActor = luacore.si_actor_create(i32(actorType), i64(actorID))
    if not cppActor then return nil end

    luaInstance._cpp_actor = cppActor
    luaInstance._lua_ref = luaInstance

    -- Add to C++ ActorMgr BEFORE install so Find() can locate it
    luacore.si_actor_mgr_add_instance(cppActor)

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

    -- Lua 业务卸载 (表实例 onUnInstall)
    if type(instance.onUnInstall) == "function" then
        instance:onUnInstall()
    end

    -- 清理本地 Lua 索引
    if localInstances[actorType] then
        localInstances[actorType][actorID] = nil
        if next(localInstances[actorType]) == nil then
            localInstances[actorType] = nil
        end
    end

    -- C++ 卸载环境 + 释放内存 (DeleteActor: Find → UnInstall → delete)
    -- 不再额外调用 uninstall(), 避免二次 UnInstall (原路径会重复触发 si_actor_mgr_uninstall)
    local ok = luacore.si_actor_creator_delete(i32(actorType), i64(actorID))

    xEngine.Log.info("[ActorMgr] Local delete: type=%d id=%d ok=%s", actorType, actorID, tostring(ok))
    return ok
end

function xEngine.ActorMgr.getByType(actorType)
    return localInstances[actorType] or {}
end

-- 遍历所有本地实例 (type, id, instance) → callback(type, id, instance)。
-- 先对 type/id 拍快照再回调, 避免回调内部增删 localInstances 破坏迭代器 (灰度迁移 saveAll)。
function xEngine.ActorMgr.forEachInstance(callback)
    if type(callback) ~= "function" then return end
    for actorType, instances in pairs(localInstances) do
        for actorID, instance in pairs(instances) do
            callback(actorType, actorID, instance)
        end
    end
end

function xEngine.ActorMgr.getInstanceCount()
    return luacore.si_actor_mgr_get_instance_count()
end

return xEngine.ActorMgr
