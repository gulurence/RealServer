-- entity.lua — Handle entity events
local PlayerClass = xEngine.Player

function PlayerClass:on_entity_event(event_enum)
    xEngine.Log.info("[Player] on_entity_event: %d", event_enum)
end
