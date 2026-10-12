-- player.lua — Player actor implementation
local AT = xEngine.ActorType

-- 玩家模块消息 ID (镜像 player.proto PlayerMsgId)
local PlayerMsgId = {
    ENTER_REQ        = 1,
    ENTER_RSP        = 2,
    LEAVE_REQ        = 3,
    LEAVE_RSP        = 4,
    SAVE_LOADOUT_REQ = 5,   -- 客户端上传/持久化枪械载具 (服务端权威)
    DISCONNECT       = 6,   -- 服务端内部断线通知 (Gate 转发, 非客户端消息)
}

local PlayerClass = xEngine.Class.define("Player", xEngine.ActorBase)

function PlayerClass:onInstall()
    xEngine.Log.info("[Player] onInstall")
    return true
end

function PlayerClass:onUnInstall()
    xEngine.Log.info("[Player] onUnInstall")
    return true
end

-- 装备从 Redis player:equip:{roleId} 读回 (断线重连/跨进程复用), 格式 "primaryGun|g0,...,g5"。
-- 权威跨进程读由 Room 直接读 Redis (本 Actor 内存缓存仅作本机复用)。
function PlayerClass:data_load(actor_data)
    local roleId = self._actor_id
    local raw = xEngine.Redis.Get("player:equip:" .. xEngine.Number.intKey(roleId))
    local primaryGun, equip = raw:match("^(%d+)|(.+)$")
    self.primaryGun = primaryGun and tonumber(primaryGun) or 0
    self.equip      = equip or "22,22,22,22,22,22"
    return true
end

function PlayerClass:data_save(save_type)
    return true
end

function PlayerClass:on_event(event_enum)
end

function PlayerClass:onInit()
    return true
end

function PlayerClass:onCreate()
    return true
end

--------------------------------------------------------------------------------
-- 业务消息总入口 (Gate 转发而来; 目标 id = 发送方自身 player_id)
--------------------------------------------------------------------------------
function PlayerClass:on_rpc_forward(wrapped)
    local senderId  = xEngine.Number.toInt(luacore.si_net_body_cid(wrapped))
    local payload   = luacore.si_net_body_payload(wrapped)
    local smallType = luacore.si_pb_small_type(payload)

    if smallType == PlayerMsgId.SAVE_LOADOUT_REQ then
        return self:handle_save_loadout(senderId, payload)
    elseif smallType == PlayerMsgId.DISCONNECT then
        return self:handle_disconnect(senderId)
    end

    xEngine.Log.warn("[Player] unknown small_type=%d", smallType)
    return ""
end

-- 断线收尾 (Gate 转发): 玩家离线状态落 frame 状态字段 online=0 (心跳落盘, 可恢复)。
-- Player Actor 不卸载 (断线重连/重复选角复用同一实例), 仅置状态。
function PlayerClass:handle_disconnect(senderId)
    local me = luacore.si_actor_mgr_find(AT.PLAYER, self._actor_id)
    if me then
        luacore.si_set_state_field(me, "online", "0")
    end
    xEngine.Log.info("[Player] disconnect: player=%d -> offline", self._actor_id)
    return ""
end

-- 上传枪械载具: 客户端选角后进大厅发一次, 服务端权威持久化到 Redis (与 Role/Account 同档)。
-- 开局时 Room 读 player:equip:{pid} 组装 battle:equip:{battleId} 下发给 Battle/客户端。
function PlayerClass:handle_save_loadout(senderId, payload)
    local equip      = luacore.si_pb_string_field(payload, 3)
    local primaryGun = xEngine.Number.toInt(luacore.si_pb_varint_field(payload, 4))

    -- 主枪内核类别校验 (kGun*: 0/1/2/3/16/17), 非法回退手枪 0
    local validGun = { [0]=true, [1]=true, [2]=true, [3]=true, [16]=true, [17]=true }
    if not validGun[primaryGun] then primaryGun = 0 end

    -- 6 槽载具归一: 缺槽/空值填 22 (NONE), 超过 6 槽截断
    local slots = {}
    if equip and equip ~= "" then
        for v in equip:gmatch("[^,]+") do slots[#slots + 1] = v end
    end
    while #slots < 6 do slots[#slots + 1] = "22" end
    if #slots > 6 then for i = 7, #slots do slots[i] = nil end end
    equip = table.concat(slots, ",")

    self.equip      = equip
    self.primaryGun = primaryGun

    local roleId = self._actor_id
    xEngine.Redis.Set("player:equip:" .. xEngine.Number.intKey(roleId),
        xEngine.Number.intKey(primaryGun) .. "|" .. equip)

    xEngine.Log.info("[Player] save_loadout: player=%d primary=%d equip=%s", roleId, primaryGun, equip)
    return ""
end

-- 战斗结算回传 (BattleActor 经 si_actor_post_message 投递 MSG.PLAYER_SETTLE):
--   body = "dExp|dGold|cost|rank"
-- 应用数值增量到角色属性 (xEngine.Role.updateAttrs, 写回 Redis)。攻击/防御/血量
-- 属展示属性, 结算只动 exp/gold; 金币入场费已在 dGold 中扣减过。
function PlayerClass:on_player_settle_msg(raw_body)
    local fields = {}
    for v in (raw_body or ""):gmatch("[^|]+") do fields[#fields + 1] = v end
    local dExp  = tonumber(fields[1]) or 0
    local dGold = tonumber(fields[2]) or 0
    local cost  = tonumber(fields[3]) or 0
    local rank  = tonumber(fields[4]) or 0

    local roleId = self._actor_id
    local attrs  = xEngine.Role.updateAttrs(roleId, { exp = dExp, gold = dGold })
    xEngine.Log.info("[Player] settle: player=%d dExp=%d dGold=%d cost=%d rank=%d -> exp=%d gold=%d",
        roleId, dExp, dGold, cost, rank, attrs.exp, attrs.gold)
end

xEngine.ActorMgr.registerPrototype(AT.PLAYER, PlayerClass)
xEngine.ActorMgr.install(AT.PLAYER, 0)
