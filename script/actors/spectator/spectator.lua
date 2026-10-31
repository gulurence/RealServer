--[[
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: spectator.lua
      Purpose:   观战 Actor 实现（多实例，Battle 进程）

      SpectatorClass 与 BattleActor 一一对应（id=battle_id），处理观战 ENTER_REQ/LEAVE_REQ。
      handle_enter 校验战斗房并从 Redis 重建花名册/装备/关卡，登记观战席位到 battle:spectator:{battleId}；席位清空后经 SPECTATOR_FINALIZE 延迟回收自删。
]]

-- spectator.lua — Spectator actor implementation (多实例, Battle 进程, id=battle_id)
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
    self.spectators = {}           -- pid -> true
    self.finalizeScheduled = false -- 空闲回收定时器是否已排 (防重复)
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

-- 请求观战: 登记观战席位 → 回 ENTER_RSP (携带战斗房数据 + 直连 host:port, 客户端据此直连 Battle 进程)。
-- 权威观战名单登记到 Redis battle:spectator:{battleId} (逗号分隔 pid), 由 Battle 进程的
-- BattleActor 读取并向直连观战者推快照/帧流/结束 (不经 Gate)。
function SpectatorClass:handle_enter(senderId, payload)
    local battleId = self._actor_id

    -- 校验战斗房仍存在 (battle:roster 为空 = 战斗已结束/不存在), 拒绝进入而非渲染假花名册
    local roster = xEngine.Redis.Get("battle:roster:" .. xEngine.Number.intKey(battleId))
    if not roster or roster == "" then
        xEngine.Log.warn("[Spectator] enter: battle=%d roster empty, reject", battleId)
        return luacore_spectate.si_pb_spectate_enter_rsp(EC.BATTLE_NOT_FOUND, battleId, 0, "", 0, 0, "", 0)
    end

    self.spectators[senderId] = true
    local key  = "battle:spectator:" .. xEngine.Number.intKey(battleId)
    local list = {}
    for v in (xEngine.Redis.Get(key) or ""):gmatch("[^,]+") do
        if v ~= "" then list[#list + 1] = v end
    end
    list[#list + 1] = xEngine.Number.intKey(senderId)
    xEngine.Redis.Set(key, table.concat(list, ","))
    -- 反向索引: 观战者 → 战斗房 (断线收尾时 Gate 据此定位并移除观战席位; TTL 兜底防残留)
    local byPlayerKey = "battle:spectator:byplayer:" .. xEngine.Number.intKey(senderId)
    xEngine.Redis.Set(byPlayerKey, xEngine.Number.intKey(battleId))
    xEngine.Redis.Expire(byPlayerKey, 86400)

    local seed = (xEngine.Number.toInt(battleId) % 0xFFFFFFFF) + 1
    local playersStr, levelId, difficulty = self:build_players_str(battleId)

    -- 战斗进程直连地址 (room.lua handle_start 写入 battle:host:{battleId} = "ip:listen_port"),
    -- 观战者据此直连 Battle 进程握手追帧 (与参战者同一套 203 帧流)。
    local hostIp, port = "", 0
    local addr = xEngine.Redis.Get("battle:host:" .. xEngine.Number.intKey(battleId)) or ""
    if addr ~= "" then
        local ip, p = addr:match("^([^:]+):(%d+)$")
        if ip and p then hostIp, port = ip, tonumber(p) or 0 end
    end

    xEngine.Log.info("[Spectator] enter: battle=%d spectator=%d seed=%d level=%d diff=%d host=%s:%d",
        battleId, senderId, seed, levelId, difficulty, hostIp, port)
    self:update_report()
    return luacore_spectate.si_pb_spectate_enter_rsp(EC.NONE, battleId, seed, playersStr, levelId, difficulty, hostIp, port)
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
    xEngine.Redis.Del("battle:spectator:byplayer:" .. sid)
    if #list == 0 then
        self:schedule_reclaim()   -- 观战席清空 → 空闲回收 (延迟失效时长后自删)
    end
    xEngine.Log.info("[Spectator] leave: battle=%d spectator=%d", battleId, senderId)
    self:update_report()
    return luacore_spectate.si_pb_spectate_leave_rsp(EC.NONE)
end

-- 监控上报: 生成本场观战席状态 JSON 并缓存到 C++ Actor (与 battle.lua update_report 同构)。
-- SpectatorActor 经 ActorCreator 跨进程创建 (无 _cpp_actor), 这里用 (type,id) 定位本进程的
-- C++ Actor; 心跳线程 Sweep 每秒读该缓存拼进监控 Redis 键, WebPlatform「Actor 实例」页据此
-- 展示观战战斗房 id / 观战名单 / 观战人数。观战席仅在 enter/leave 时变化, 故变化后立即写一次。
function SpectatorClass:update_report()
    local battleId = xEngine.Number.toInt(self._actor_id)
    local list = {}
    for v in (xEngine.Redis.Get("battle:spectator:" .. xEngine.Number.intKey(self._actor_id)) or ""):gmatch("[^,]+") do
        if v ~= "" then list[#list + 1] = v end
    end
    local ids = {}
    for _, v in ipairs(list) do ids[#ids + 1] = xEngine.Number.intKey(v) end
    local json = string.format('{"battle_id":%d,"spectator_count":%d,"spectators":[%s]}',
        battleId, #list, table.concat(ids, ","))
    luacore.si_actor_set_report_extra(self._actor_type, self._actor_id, json)
end

--------------------------------------------------------------------------------
-- 空闲回收: 观战席清空后延迟「Actor 状态失效时长」自删 (对齐 battle.lua 的
-- BATTLE_FINALIZE 延迟回收模式)。延迟读 WebPlatform「服务器基础配置 → Actor 状态失效
-- 时长」的 204 字段 (cluster:config:actor-state-ttl, 单位秒), 缺省 86400s。
-- 仅实例 (id≠0) 参与回收, 原型 (id=0) 不回收。到点仍空席则卸载自身, 有人回来则放弃。
--------------------------------------------------------------------------------
local kSpectatorTtlCfgKey = "cluster:config:actor-state-ttl"
local kSpectatorDefaultTtlSec = 86400

-- 回收延迟 (毫秒): 读配置 204 字段, 非法/缺失回退 86400s
function SpectatorClass:spectator_ttl_ms()
    local ttl = tonumber(xEngine.Redis.HGet(kSpectatorTtlCfgKey, tostring(AT.SPECTATOR)))
    if not ttl or ttl <= 0 then ttl = kSpectatorDefaultTtlSec end
    return ttl * 1000
end

-- 排一次回收定时器 (幂等): 到点投递 SPECTATOR_FINALIZE 到自身消息循环
function SpectatorClass:schedule_reclaim()
    if self._actor_id == 0 or self.finalizeScheduled then return end
    self.finalizeScheduled = true
    local delay = self:spectator_ttl_ms()
    luacore.si_schedule_after(AT.SPECTATOR, self._actor_id, delay,
        xEngine.MsgId.SPECTATOR_FINALIZE, "")
    xEngine.Log.info("[Spectator] reclaim scheduled battle=%d delay=%dms", self._actor_id, delay)
end

-- 延迟回收 (SPECTATOR_FINALIZE): 到点观战名单仍空 → 卸载自身; 非空 → 放弃本次回收
function SpectatorClass:on_spectator_finalize_msg(raw_body)
    local key = "battle:spectator:" .. xEngine.Number.intKey(self._actor_id)
    if (xEngine.Redis.Get(key) or "") ~= "" then
        self.finalizeScheduled = false   -- 延迟窗口内有人回来, 待其离开后再触发
        xEngine.Log.info("[Spectator] reclaim aborted: spectators present battle=%d", self._actor_id)
        return
    end
    xEngine.Redis.Del(key)
    luacore.si_actor_delete_local(AT.SPECTATOR, self._actor_id)
    xEngine.Log.info("[Spectator] reclaim done: battle=%d", self._actor_id)
end

xEngine.ActorMgr.registerPrototype(AT.SPECTATOR, SpectatorClass)
xEngine.ActorMgr.install(AT.SPECTATOR, 0)
