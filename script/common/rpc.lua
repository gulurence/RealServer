-- rpc.lua — xEngine.RPC global tool module
-- Wraps C++ RPC/gRPC operations exported via SWIG si_rpc_* functions.
--
-- Usage:
--   require("common.rpc")
--   xEngine.RPC.Send(target_actor_id, serialized_msg)

xEngine = xEngine or {}
xEngine.RPC = {}

function xEngine.RPC.Send(actor_id, msg)
    return luacore.si_rpc_send(actor_id, msg)
end
