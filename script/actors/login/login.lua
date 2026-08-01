-- login.lua — Login actor implementation
local AT = xEngine.ActorType
xEngine.ActorMgr.hostType(AT.LOGIN)

local LoginClass = xEngine.Class.define("Login", xEngine.ActorBase)

function LoginClass:onInstall()
    xEngine.Log.info("[Login] onInstall")
    return true
end

function LoginClass:onUnInstall()
    xEngine.Log.info("[Login] onUnInstall")
    return true
end

function LoginClass:data_load(actor_data)
    return true
end

function LoginClass:data_save(save_type)
    return true
end

function LoginClass:on_event(event_enum)
end

function LoginClass:on_msg(proto_msg)
end

function LoginClass:onInit()
    return true
end

function LoginClass:onCreate()
    return true
end

xEngine.ActorMgr.registerPrototype(AT.LOGIN, LoginClass)
xEngine.ActorMgr.install(AT.LOGIN, 0)
