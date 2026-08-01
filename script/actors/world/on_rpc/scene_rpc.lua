-- scene_rpc.lua — Handle RPC scene operations
local WorldClass = xEngine.World

function WorldClass:on_rpc_scene_op(msg)
    xEngine.Log.info("[World] on_rpc_scene_op")
end
