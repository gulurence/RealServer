-- gate.lua — Gate actor implementation
local AT = xEngine.ActorType
xEngine.ActorMgr.hostType(AT.GATE)

local GateClass = xEngine.Class.define("Gate", xEngine.ActorBase)

function GateClass:onInstall()
    xEngine.Log.info("[Gate] onInstall — connections ready")
    return true
end

function GateClass:onUnInstall()
    xEngine.Log.info("[Gate] onUnInstall")
    return true
end

function GateClass:data_load(actor_data)
    return true
end

function GateClass:data_save(save_type)
    return true
end

function GateClass:on_event(event_enum)
end

function GateClass:on_msg(proto_msg)
end

function GateClass:onInit()
    return true
end

function GateClass:onCreate()
    return true
end

xEngine.ActorMgr.registerPrototype(AT.GATE, GateClass)
xEngine.ActorMgr.install(AT.GATE, 0)
