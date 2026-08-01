-- player.lua — Player actor implementation
local AT = xEngine.ActorType
xEngine.ActorMgr.hostType(AT.PLAYER)

local PlayerClass = xEngine.Class.define("Player", xEngine.ActorBase)

function PlayerClass:onInstall()
    xEngine.Log.info("[Player] onInstall")
    return true
end

function PlayerClass:onUnInstall()
    xEngine.Log.info("[Player] onUnInstall")
    return true
end

function PlayerClass:data_load(actor_data)
    return true
end

function PlayerClass:data_save(save_type)
    return true
end

function PlayerClass:on_event(event_enum)
end

function PlayerClass:on_msg(proto_msg)
end

function PlayerClass:onInit()
    return true
end

function PlayerClass:onCreate()
    return true
end

xEngine.ActorMgr.registerPrototype(AT.PLAYER, PlayerClass)
xEngine.ActorMgr.install(AT.PLAYER, 0)
