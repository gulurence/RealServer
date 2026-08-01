-- verify.lua — Handle RPC authentication verify
local LoginClass = xEngine.Login

function LoginClass:on_rpc_verify(msg)
    xEngine.Log.info("[Login] on_rpc_verify")
end
