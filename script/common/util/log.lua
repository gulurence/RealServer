--[[
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: log.lua
      Purpose:   日志桥接模块

      xEngine.Log 桥接 C++ xLog 系统，提供 trace/debug/info/warn/error 五个日志级别，经 si_log_* 输出。
]]

-- log.lua — Bridge to C++ xLog system via xEngine.Log
-- Uses the si_log_* helpers registered into the luacore table by SWIG.
--
-- Usage:
--   xEngine.Log.trace("message %s", value)
--   xEngine.Log.info("message %s", value)
--   xEngine.Log.warn("message %s", value)
--   xEngine.Log.error("message %s", value)

xEngine = xEngine or {}
xEngine.Log = {}

function xEngine.Log.trace(fmt, ...)
    if select("#", ...) > 0 then
        luacore.si_log_trace(string.format(fmt, ...))
    else
        luacore.si_log_trace(fmt)
    end
end

function xEngine.Log.debug(fmt, ...)
    if select("#", ...) > 0 then
        luacore.si_log_debug(string.format(fmt, ...))
    else
        luacore.si_log_debug(fmt)
    end
end

function xEngine.Log.info(fmt, ...)
    if select("#", ...) > 0 then
        luacore.si_log_info(string.format(fmt, ...))
    else
        luacore.si_log_info(fmt)
    end
end

function xEngine.Log.warn(fmt, ...)
    if select("#", ...) > 0 then
        luacore.si_log_warn(string.format(fmt, ...))
    else
        luacore.si_log_warn(fmt)
    end
end

function xEngine.Log.error(fmt, ...)
    if select("#", ...) > 0 then
        luacore.si_log_error(string.format(fmt, ...))
    else
        luacore.si_log_error(fmt)
    end
end
