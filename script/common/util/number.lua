--[[
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: number.lua
      Purpose:   整型 ID 归一化工具

      xEngine.Number 提供 toInt/intKey，把 SWIG 暴露的浮点整数值归一化为 Lua 整数，避免 Redis 键出现 3.0 之类格式。
      用于拼接 Redis 键与日志。
]]

-- number.lua — 数值工具: 归一化跨 SWIG 的整型 ID
--
-- 背景: SWIG 生成的 si_* 接口把所有 C++ 整型 (int32/int64) 都以
-- lua_pushnumber(double) 暴露给 Lua, 于是整数值 ID 在 Lua 里是浮点数 3.0,
-- tostring(3.0) == "3.0", 导致 Redis 键变成 account:3.0:state 而非 account:3:state。
--
-- 这里提供 toInt / intKey, 把「整数值的浮点数」归一化为 Lua 整数, 保证键名正确。
--
-- Usage:
--   xEngine.Number.intKey(3.0)  --> "3"
--   xEngine.Number.toInt(3.0)   --> 3

xEngine = xEngine or {}
xEngine.Number = {}

--- 把 number 归一化为整数 (float 整数值 → integer; 非整数 → floor 兜底)
-- @param v number|any
-- @return integer|any  (非 number 原样返回)
function xEngine.Number.toInt(v)
    if type(v) == "number" then
        local i = math.tointeger(v)
        if i then
            return i
        end
        return math.floor(v)
    end
    return v
end

--- 归一化后转字符串 (用于拼接 Redis 键 / 日志, 避免 "3.0")
-- @param v number|any
-- @return string
function xEngine.Number.intKey(v)
    return tostring(xEngine.Number.toInt(v))
end
