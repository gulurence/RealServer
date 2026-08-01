-- forward.lua — Handle RPC forwarded from other processes
local GateClass = xEngine.Gate

function GateClass:on_rpc_forward(msg)
    xEngine.Log.info("[Gate] on_rpc_forward")
    -- TODO: forward message to target client connection
end
