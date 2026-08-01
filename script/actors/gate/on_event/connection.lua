-- connection.lua — Handle connection events
local GateClass = xEngine.Gate

function GateClass:on_connection_event(event_enum)
    xEngine.Log.info("[Gate] on_connection_event: %d", event_enum)
    -- TODO: handle client connect/disconnect/timeout
end
