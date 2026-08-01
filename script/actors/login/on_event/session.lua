-- session.lua — Handle session events
local LoginClass = xEngine.Login

function LoginClass:on_session_event(event_enum)
    xEngine.Log.info("[Login] on_session_event: %d", event_enum)
end
