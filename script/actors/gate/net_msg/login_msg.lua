-- login_msg.lua -- Handle login proto message
local GateClass = xEngine.Gate

function GateClass:on_login_msg(msg)
    xEngine.Log.info("[Gate] on_login_msg")
    -- TODO: deserialize msg, authenticate, create session
end
