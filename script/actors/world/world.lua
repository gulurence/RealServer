-- world.lua — World actor implementation
local AT = xEngine.ActorType
xEngine.ActorMgr.hostType(AT.WORLD)

local WorldClass = xEngine.Class.define("World", xEngine.ActorBase)

function WorldClass:onInstall()
    xEngine.Log.info("[World] onInstall")
    return true
end

function WorldClass:onUnInstall()
    xEngine.Log.info("[World] onUnInstall")
    return true
end

function WorldClass:data_load(actor_data)
    return true
end

function WorldClass:data_save(save_type)
    return true
end

function WorldClass:on_event(event_enum)
end

function WorldClass:on_msg(proto_msg)
end

function WorldClass:onInit()
    return true
end

function WorldClass:onCreate()
    return true
end

xEngine.ActorMgr.registerPrototype(AT.WORLD, WorldClass)
xEngine.ActorMgr.install(AT.WORLD, 0)
