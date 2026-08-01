-- data_sync.lua — Handle RPC data sync from other processes
local PlayerClass = xEngine.Player

function PlayerClass:on_rpc_data_sync(msg)
    xEngine.Log.info("[Player] on_rpc_data_sync")
end
