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

--- 计数器: INCR / DECR (返回自增/自减后的值, Redis 不可用返回 -1)
function xEngine.Redis.Incr(key)
    return luacore.si_redis_incr(key)
end

function xEngine.Redis.Decr(key)
    return luacore.si_redis_decr(key)
end

--- 哈希: HGET / HSET / HGETALL
function xEngine.Redis.HGet(hash, field)
    return luacore.si_redis_hget(hash, field)
end

function xEngine.Redis.HSet(hash, field, value)
    return luacore.si_redis_hset(hash, field, value)
end

--- 列表: RPUSH / LPOP / LPOS / LLEN (登录排队 FIFO)
function xEngine.Redis.RPush(key, value)
    return luacore.si_redis_rpush(key, value)
end

function xEngine.Redis.LPop(key)
    return luacore.si_redis_lpop(key)
end

function xEngine.Redis.LPos(key, member)
    return luacore.si_redis_lpos(key, member)
end

function xEngine.Redis.LLen(key)
    return luacore.si_redis_llen(key)
end

--- SETNXEX / EXPIRE (每账号登录短锁 + session 续期)
function xEngine.Redis.SetNxEx(key, value, ttlSec)
    return luacore.si_redis_setnxex(key, value, ttlSec)
end

function xEngine.Redis.Expire(key, ttlSec)
    return luacore.si_redis_expire(key, ttlSec)
end

--- 集合: SADD / SREM / SMEMBERS (下线列表 offline:players + 灰度迁移 actor:index:*)
-- SMEMBERS 由 C++ join 成逗号串返回 (成员均为整数 id, 不含逗号), 调用方自行 split。
function xEngine.Redis.SAdd(key, member)
    return luacore.si_redis_sadd(key, member)
end

function xEngine.Redis.SRem(key, member)
    return luacore.si_redis_srem(key, member)
end

function xEngine.Redis.SMembers(key)
    return luacore.si_redis_smembers(key)
end
