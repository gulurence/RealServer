-- spectator.lua — Spectator actor implementation (多实例, Global 进程, id=battle_id)
--
-- 观战玩家只读帧流、不参与模拟: 进入时快照对齐, 之后追帧播放。
-- 同一战斗房的所有观战者共享一个 SpectatorActor 实例 (id == battle_id)。
--
-- SNAPSHOT_NTF / FRAME_NTF / END_NTF 是服务器主动推送 (需 Gate→客户端 push 链路),
-- 当前骨架未接推送, 进入/退出仅登记观战席位; 客户端回包走 on_rpc_forward 返回值。

local AT = xEngine.ActorType

-- 观战模块消息 ID (镜像 spectator.proto SpectatorMsgId)
local SpectatorMsgId = {
    ENTER_REQ    = 1, ENTER_RSP    = 2, LEAVE_REQ   = 3, LEAVE_RSP    = 4,
    SNAPSHOT_NTF = 5, FRAME_NTF    = 6, END_NTF     = 7,
}

-- 错误码
local EC = { NONE = 0, SESSION_INVALID = 1001, INTERNAL = 1999, BATTLE_NOT_FOUND = 2201 }

local SpectatorClass = xEngine.Class.define("Spectator", xEngine.ActorBase)

--------------------------------------------------------------------------------
-- 构造: 每个实例独立状态
--------------------------------------------------------------------------------
function SpectatorClass:ctor(actor_type, actor_id_or_name, actor_type_name)
    self:super("ctor", actor_type, actor_id_or_name, actor_type_name)
    self.spectators = {}   -- pid -> true
end

function SpectatorClass:onInstall()
    xEngine.Log.info("[Spectator] onInstall id=%d", self._actor_id)
    return true
end

function SpectatorClass:onUnInstall()
    xEngine.Log.info("[Spectator] onUnInstall id=%d", self._actor_id)
    return true
end

function SpectatorClass:data_load(actor_data) return true end
function SpectatorClass:data_save(save_type) return true end
function SpectatorClass:onInit()    return true end
function SpectatorClass:onCreate()  return true end

--------------------------------------------------------------------------------
-- 业务消息总入口
--------------------------------------------------------------------------------
function SpectatorClass:on_rpc_forward(wrapped)
    local senderId  = xEngine.Number.toInt(luacore.si_net_body_cid(wrapped))
    local payload   = luacore.si_net_body_payload(wrapped)
    local smallType = luacore.si_pb_small_type(payload)

    if smallType == SpectatorMsgId.ENTER_REQ then
        return self:handle_enter(senderId, payload)
    elseif smallType == SpectatorMsgId.LEAVE_REQ then
        return self:handle_leave(senderId, payload)
    end

    xEngine.Log.warn("[Spectator] unknown small_type=%d", smallType)
    return ""
end

-- 从 Redis 重建战斗房花名册 (镜像 battle.lua ensure_core/players_str 的组装逻辑):
-- 观战者不参与模拟, 但进入战斗场景需要与参战者完全一致的花名册/种子/关卡数据。
-- 返回 playersStr ("pid,name,spawnIndex,primaryGun,level,exp,gold,attack,defense,hp,g0..g5;..."),
--       levelId, difficulty。
function SpectatorClass:build_players_str(battleId)
    local roster = xEngine.Redis.Get("battle:roster:" .. xEngine.Number.intKey(battleId)) or ""
    if roster == "" then roster = "0" end

    local lvlId, lvlDiff =
        (xEngine.Redis.Get("battle:level:" .. xEngine.Number.intKey(battleId)) or ""):match("^(%d+),(%d+)$")
    local levelId    = lvlId and tonumber(lvlId) or 11
    local difficulty = lvlDiff and tonumber(lvlDiff) or 0

    -- 玩家装备 (局外系统经 Room 写入 battle:equip:{battleId} = "pid:primaryGun:g0,...,g5;..."):
    -- 读回解析成 pid → {primaryGun, equip}; 无装备回退 0 + 全空载具。
    local equipStr = xEngine.Redis.Get("battle:equip:" .. xEngine.Number.intKey(battleId)) or ""
    local equipMap = {}
    for rec in (equipStr .. ";"):gmatch("([^;]*);") do
        local pidStr, gunStr, guns = rec:match("^(%d+):(%d+):(.+)$")
        if pidStr and gunStr then
            equipMap[tonumber(pidStr)] = { tonumber(gunStr), guns or "22,22,22,22,22,22" }
        end
    end

    local parts, idx = {}, 0
    for pidStr in (roster .. ","):gmatch("([^,]*),") do
        if pidStr ~= "" then
            local pid = tonumber(pidStr)
            local eq  = equipMap[pid]
            local attrs = xEngine.Role.parseAttrs(pid)
            local name = (attrs.name and attrs.name ~= "") and attrs.name or ("player_" .. pidStr)
            parts[#parts + 1] = table.concat({
                xEngine.Number.intKey(pid), name, tostring(idx),
                tostring(eq and eq[1] or 0), tostring(attrs.level or 1),
                tostring(attrs.exp or 0), tostring(attrs.gold or 0),
                tostring(attrs.attack or 10), tostring(attrs.defense or 0), tostring(attrs.hp or 100),
                (eq and eq[2]) or "22,22,22,22,22,22",
            }, ",")
            idx = idx + 1
        end
    end
    return table.concat(parts, ";"), levelId, difficulty
end

-- 请求观战: 登记观战席位 → 回 ENTER_RSP (携带战斗房数据, 客户端据此进入战斗场景)。
-- 权威观战名单登记到 Redis battle:spectator:{battleId} (逗号分隔 pid), 由 Game 进程的
-- BattleActor 每 10 帧读取并向观战者推快照/帧流/结束 (跨进程经 Redis 共享)。
function SpectatorClass:handle_enter(senderId, payload)
    local battleId = self._actor_id

    -- 校验战斗房仍存在 (battle:roster 为空 = 战斗已结束/不存在), 拒绝进入而非渲染假花名册
    local roster = xEngine.Redis.Get("battle:roster:" .. xEngine.Number.intKey(battleId))
    if not roster or roster == "" then
        xEngine.Log.warn("[Spectator] enter: battle=%d roster empty, reject", battleId)
        return luacore_spectate.si_pb_spectate_enter_rsp(EC.BATTLE_NOT_FOUND, battleId, 0, "", 0, 0)
    end

    self.spectators[senderId] = true
    local key  = "battle:spectator:" .. xEngine.Number.intKey(battleId)
    local list = {}
    for v in (xEngine.Redis.Get(key) or ""):gmatch("[^,]+") do
        if v ~= "" then list[#list + 1] = v end
    end
    list[#list + 1] = xEngine.Number.intKey(senderId)
    xEngine.Redis.Set(key, table.concat(list, ","))

    local seed = (xEngine.Number.toInt(battleId) % 0xFFFFFFFF) + 1
    local playersStr, levelId, difficulty = self:build_players_str(battleId)
    xEngine.Log.info("[Spectator] enter: battle=%d spectator=%d seed=%d level=%d diff=%d",
        battleId, senderId, seed, levelId, difficulty)
    return luacore_spectate.si_pb_spectate_enter_rsp(EC.NONE, battleId, seed, playersStr, levelId, difficulty)
end

-- 退出观战
function SpectatorClass:handle_leave(senderId, payload)
    local battleId = self._actor_id
    self.spectators[senderId] = nil
    local key  = "battle:spectator:" .. xEngine.Number.intKey(battleId)
    local sid  = xEngine.Number.intKey(senderId)
    local list = {}
    for v in (xEngine.Redis.Get(key) or ""):gmatch("[^,]+") do
        if v ~= "" and v ~= sid then list[#list + 1] = v end
    end
    if #list > 0 then
        xEngine.Redis.Set(key, table.concat(list, ","))
    else
        xEngine.Redis.Del(key)
    end
    xEngine.Log.info("[Spectator] leave: battle=%d spectator=%d", battleId, senderId)
    return luacore_spectate.si_pb_spectate_leave_rsp(EC.NONE)
end

xEngine.ActorMgr.registerPrototype(AT.SPECTATOR, SpectatorClass)
xEngine.ActorMgr.install(AT.SPECTATOR, 0)
