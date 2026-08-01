-- redis.lua — xEngine.Redis global tool module
-- Wraps C++ Redis operations exported via SWIG si_redis_* functions.
--
-- Usage:
--   require("common.redis")
--   local val = xEngine.Redis.Get("player:100")
--   xEngine.Redis.Set("player:100", data)
--   xEngine.Redis.Del("player:100")

xEngine = xEngine or {}
xEngine.Redis = {}

function xEngine.Redis.Get(key)
    return luacore.si_redis_get(key)
end

function xEngine.Redis.Set(key, value)
    return luacore.si_redis_set(key, value)
end

function xEngine.Redis.Del(key)
    return luacore.si_redis_del(key)
end
