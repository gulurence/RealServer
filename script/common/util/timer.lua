-- timer.lua — Wraps C++ xTimer/xMilliTimer via SWIG luacore, registered as xEngine.Timer
--
-- Usage:
--   local t = xEngine.Timer.new_second(5)
--   if t:timeUp() then ... end

xEngine = xEngine or {}
xEngine.Timer = {}

function xEngine.Timer.new_second(threshold)
    return luacore.si_timer_wrap(threshold)
end

function xEngine.Timer.new_milli(threshold)
    return luacore.si_milli_timer_wrap(threshold)
end
