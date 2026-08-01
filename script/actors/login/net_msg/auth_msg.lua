-- auth_msg.lua — Handle auth proto message
local LoginClass = xEngine.Login

function LoginClass:on_auth_msg(msg)
    xEngine.Log.info("[Login] on_auth_msg")
end
