-- class.lua — Lua OOP class management system
-- Provides class definition with inheritance, constructor, super calls,
-- and global registry for hot-reload support.
-- Registered as xEngine.Class.
--
-- Usage:
--   require("common.class")
--   local Animal = xEngine.Class.define("Animal")
--   function Animal:ctor(name) self._name = name end
--   local Dog = xEngine.Class.define("Dog", xEngine.Animal)
--   local dog = xEngine.Class.create("Dog", "Rex")
--
-- Hot-reload: re-executing common/include.lua re-runs define() calls which
-- update class tables in-place; existing instances automatically get new
-- method definitions via their metatable.__index pointer.

xEngine = xEngine or {}

--------------------------------------------------------------------------------
-- Internal class registry
--------------------------------------------------------------------------------
local ClassRegistry = {}

--------------------------------------------------------------------------------
-- xEngine.Class — the class management facade
--------------------------------------------------------------------------------
xEngine.Class = {}

--- Define a new class (or update an existing one for hot-reload).
-- @param name       string  Class name; also registered as xEngine.<name>
-- @param parent     table   (optional) Parent class table
-- @return           table   The class table (also accessible as xEngine.<name>)
function xEngine.Class.define(name, parent)
    local cls = ClassRegistry[name]

    if cls then
        -- Hot-reload path: update existing class table in-place
        cls._parent = parent

        for k, _ in pairs(cls) do
            if k ~= '_className' and k ~= '_parent' and k ~= '__index'
               and k ~= 'new' and k ~= 'super' then
                cls[k] = nil
            end
        end

        if parent then
            setmetatable(cls, { __index = parent })
        else
            setmetatable(cls, nil)
        end
    else
        -- First-load path
        cls = {}
        cls._className = name
        cls._parent = parent
        cls.__index = cls

        if parent then
            setmetatable(cls, { __index = parent })
        end

        ClassRegistry[name] = cls
        xEngine[name] = cls
    end

    -- Instance constructor
    function cls:new(...)
        local obj = setmetatable({}, self)
        obj._class = self
        if self.ctor then
            self.ctor(obj, ...)
        end
        return obj
    end

    -- Super call helper
    function cls:super(methodName, ...)
        local parent = self._class._parent
        if not parent then
            error(string.format(
                "[Class:%s] No parent class to call super.%s",
                self._class._className, methodName))
        end
        local method = parent[methodName]
        if not method then
            error(string.format(
                "[Class:%s] Parent method '%s' not found",
                self._class._className, methodName))
        end
        return method(self, ...)
    end

    return cls
end

--- Create an instance by class name.
function xEngine.Class.create(name, ...)
    local cls = ClassRegistry[name]
    if not cls then
        error("[Class] Class not found: " .. name)
    end
    return cls:new(...)
end

--- Get the class table by name.
function xEngine.Class.get(name)
    return ClassRegistry[name]
end

--- Check whether a class has been registered.
function xEngine.Class.exists(name)
    return ClassRegistry[name] ~= nil
end

return xEngine.Class
