--[[
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: string_ext.lua
      Purpose:   字符串扩展工具

      xEngine.StringExt 封装 C++ StringTool，提供 trim/trim_begin/trim_end/contains/starts_with/ends_with/to_lower/to_upper/replace。
]]

-- string_ext.lua — Wraps C++ StringTool via SWIG luacore, registered as xEngine.StringExt
--
-- Usage:
--   xEngine.StringExt.trim("  hello  ")  --> "hello"

xEngine = xEngine or {}
xEngine.StringExt = {}

function xEngine.StringExt.trim(s)         return luacore.si_str_trim(s) end
function xEngine.StringExt.trim_begin(s)   return luacore.si_str_trimbegin(s) end
function xEngine.StringExt.trim_end(s)     return luacore.si_str_trimend(s) end
function xEngine.StringExt.contains(s, sub)   return luacore.si_str_contains(s, sub) end
function xEngine.StringExt.starts_with(s, p)  return luacore.si_str_startswith(s, p) end
function xEngine.StringExt.ends_with(s, suf)  return luacore.si_str_endswith(s, suf) end
function xEngine.StringExt.to_lower(s)     return luacore.si_str_tolower(s) end
function xEngine.StringExt.to_upper(s)     return luacore.si_str_toupper(s) end
function xEngine.StringExt.replace(s, old, new) return luacore.si_str_replace(s, old, new) end
