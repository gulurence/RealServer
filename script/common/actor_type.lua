-- actor_type.lua — Actor type enum definitions
-- Shared between C++ (ActorType typedef) and Lua layer
-- Must match the values in engine/xBase/xDefine.h ActorType

xEngine = xEngine or {}

xEngine.ActorType = {
    -- Gate 网关 (100)
    GATE  = 100,

    -- Login 登录 (101)
    LOGIN = 101,

    -- Player 玩家 (201)
    PLAYER = 201,

    -- World 世界 (202)
    WORLD  = 202,
}
