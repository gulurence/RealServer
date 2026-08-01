-- actor_base.lua — Base class for all Lua-side actor implementations
-- Registered as xEngine.ActorBase via the class system.
--
-- Actor lifecycle (managed by xEngine.ActorMgr):
--   1. install(type, id)    — 连接管理 (进程级共享, 引用计数)
--   2. data_load(actor_data) — Redis → DB 加载 → Redis 缓存
--   3. data_save(save_type)  — 先更新 Redis, 再通知 record 模块异步落地 DB
--   4. on_event(event_enum)  — 事件处理 (event enum)
--   5. on_msg(proto_msg)     — proto 消息处理
--   6. uninstall(type, id)   — 连接引用计数递减, 归零释放连接
--
-- C++ exported interfaces use si_ prefix:
--   luacore.si_get_type(actorPtr)
--   luacore.si_get_id(actorPtr)
--   luacore.si_get_state(actorPtr)
--   luacore.si_set_state(actorPtr, state)
--   etc.
--
-- Usage:
--   local MyActor = xEngine.Class.define("MyActor", xEngine.ActorBase)
--   function MyActor:onInstall() ... return true end
--   function MyActor:data_load(data) ... return true end
--   function MyActor:data_save(save_type) ... return true end
--   xEngine.ActorMgr.registerPrototype(100, MyActor)
--   xEngine.ActorMgr.install(100, 0)

xEngine = xEngine or {}

-- xEngine.Class is loaded by common/actor/include.lua before this file

local ActorBase = xEngine.Class.define("ActorBase")

--- Constructor. Supports both 2-arg and 3-arg forms:
--   (actor_type, actor_type_name)           — legacy, id defaults to 0
--   (actor_type, actor_id, actor_type_name) — ActorMgr form
function ActorBase:ctor(actor_type, actor_id_or_name, actor_type_name)
    self._actor_type = actor_type or 0

    if actor_type_name ~= nil then
        self._actor_id = actor_id_or_name or 0
        self._actor_type_name = actor_type_name
    else
        self._actor_id = 0
        self._actor_type_name = actor_id_or_name or "Unknown"
    end

    self._cpp_actor = nil   -- opaque ActorPtr (C++ CLuaActor*)
    self._lua_ref = nil     -- Lua registry reference
end

-- =========================================================================
-- Lifecycle: Install / UnInstall (process-level connection management)
-- =========================================================================

--- Install — called when actor environment is set up.
-- At this point, DB/Redis/Log connections are already established
-- by C++ ActorMgr's connection ref counting mechanism.
-- Override for process-level initialization.
-- @return true on success
function ActorBase:onInstall()
    return true
end

--- UnInstall — called when actor environment is torn down.
-- Connections are released when reference counts reach zero.
-- Override for process-level cleanup.
-- @return true on success
function ActorBase:onUnInstall()
    return true
end

-- =========================================================================
-- Data lifecycle: data_load / data_save
-- =========================================================================

--- data_load — load actor data from Redis (or DB fallback).
-- Flow: Redis GET → if not found, DB SELECT → Redis SET for cache.
-- Called by ActorMgr when an actor instance is created or data is needed.
--
-- The raw protobuf binary is stored in self._raw_data.
-- If the subclass defines a deserialize(data) method, it is called
-- to decode the binary into typed fields.
--
-- @param actor_data  string  Protobuf binary data (may be empty for first load)
-- @return true on success
function ActorBase:data_load(actor_data)
    local key = self._actor_type_name .. ":" .. tostring(self._actor_id)

    -- Try Redis cache first
    local cached = xEngine.Redis.Get(key)
    if cached and cached ~= "" then
        self._raw_data = cached
        if type(self.deserialize) == "function" then
            self:deserialize(cached)
        end
        return true
    end

    -- Fallback to DB (async query — returns immediately)
    local dbData = xEngine.DB.Select(self._actor_type_name, "id=" .. tostring(self._actor_id))
    if dbData and dbData ~= "" then
        -- Cache the DB result to Redis for subsequent reads
        xEngine.Redis.Set(key, dbData)
        self._raw_data = dbData
        if type(self.deserialize) == "function" then
            self:deserialize(dbData)
        end
        return true
    end

    return true  -- new actor, no existing data yet
end

--- data_save — save actor data.
-- Flow:
--   1. If subclass defines serialize(), call it to get the protobuf binary.
--   2. Write to Redis cache synchronously.
--   3. Notify CRecordMgr (via si_record_notify) for async DB persistence.
--
-- DB persistence is handled asynchronously by the C++ record module.
--
-- @param save_type  int32  1=insert(新建), 2=update(更新), 3=delete(删除)
-- @return true on success
function ActorBase:data_save(save_type)
    local key = self._actor_type_name .. ":" .. tostring(self._actor_id)

    if save_type == 3 then  -- delete
        xEngine.Redis.Del(key)
    else
        -- Serialize to protobuf binary (subclass responsibility)
        local serialized = ""
        if type(self.serialize) == "function" then
            serialized = self:serialize()
        end
        xEngine.Redis.Set(key, serialized)
    end

    -- Notify C++ record module for async DB persistence
    luacore.si_record_notify(self._actor_type, self._actor_id, save_type)

    return true
end

-- =========================================================================
-- Event / Message handling
-- =========================================================================

-- Event enum → handler name suffix mapping.
-- Match the naming convention: on_<name>_event(self)
-- Extend this table when new event types are introduced.
local EVENT_NAMES = {
    [1] = "timer",        -- periodic timer tick
    [2] = "connection",   -- client connection state change
    [3] = "session",      -- session lifecycle event
    [4] = "entity",       -- entity state change
    [5] = "world_tick",   -- world simulation tick
}

--- on_event — handle an event by enum, dispatched by name.
-- Looks up the enum → name mapping, then calls self["on_" .. name .. "_event"](self).
-- If no enum mapping is found, a warning is logged.
-- @param event_enum  int32  Event type enum value
function ActorBase:on_event(event_enum)
    local name = EVENT_NAMES[event_enum]
    if not name then
        xEngine.Log.warn("[%s] Unknown event enum: %d", self._actor_type_name, event_enum)
        return
    end
    local handler = self["on_" .. name .. "_event"]
    if handler then
        handler(self, event_enum)
    end
end

--- on_msg — handle an incoming proto message, dispatched by msg type.
-- Inspects the proto_msg table for a _msg_type_ field (set by the C++/SWIG
-- deserialization layer), then calls self["on_" .. msg_type .. "_msg"](self, msg).
-- If no _msg_type_ field is found, a warning is logged.
-- @param proto_msg  table  Deserialized protobuf message with optional _msg_type_
function ActorBase:on_msg(proto_msg)
    local msg_type = nil
    if type(proto_msg) == "table" then
        msg_type = proto_msg._msg_type_ or proto_msg.msg_type
    end

    if msg_type then
        local handler = self["on_" .. msg_type .. "_msg"]
        if handler then
            return handler(self, proto_msg)
        end
        xEngine.Log.warn("[%s] No on_msg handler for type '%s'",
            self._actor_type_name, msg_type)
    else
        xEngine.Log.warn("[%s] on_msg called without _msg_type_",
            self._actor_type_name)
    end
end

--- on_rpc_call — RPC dispatch from C++ gRPC layer.
-- C++ receives gRPC call {actor_id, rpc_name, lua_table}, locates the
-- target actor, and calls this method. Automatically dispatches to
-- self["on_rpc_" .. rpc_name] matching the on_rpc/<name>.lua handler.
--
-- @param rpc_name   string  First gRPC param — locates on_rpc_<name> handler
-- @param rpc_data   string  Second gRPC param — Lua table data transparently passed
-- @return true if handled
function ActorBase:on_rpc_call(rpc_name, rpc_data)
    local method_name = "on_rpc_" .. rpc_name
    local method = self[method_name]
    if method then
        return method(self, rpc_data)
    end
    xEngine.Log.warn("[%s] No RPC handler: %s", self._actor_type_name, rpc_name)
    return false
end

-- =========================================================================
-- Instance lifecycle: onInit / onCreate
-- =========================================================================

--- onInit — per-instance initialization after data load from DB/Redis.
-- @return true on success
function ActorBase:onInit()
    return true
end

--- onCreate — per-instance post-creation hook (after onInit).
-- @return true on success
function ActorBase:onCreate()
    return true
end

-- =========================================================================
-- [DEPRECATED] Legacy register() — prefer xEngine.ActorMgr.registerPrototype()
-- =========================================================================
function ActorBase:register()
    xEngine.Log.warn("[ActorBase] register() is deprecated, use xEngine.ActorMgr.registerPrototype()")

    self._lua_ref = self

    self._cpp_actor = luacore.si_lua_actor_create(
        self._actor_type,
        self._actor_id,
        self._actor_type_name
    )

    if not self._cpp_actor then
        luacore.si_log_error(string.format(
            "[ActorBase] Failed to create CLuaActor for type=%d name=%s",
            self._actor_type, self._actor_type_name
        ))
        return false
    end

    local ok = luacore.si_actor_regist(self._actor_type, self._cpp_actor)

    luacore.si_log_info(string.format(
        "[ActorBase] Registered actor type=%d name=%s ok=%s",
        self._actor_type, self._actor_type_name, tostring(ok)
    ))

    return ok
end
