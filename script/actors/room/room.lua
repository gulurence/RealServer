-- room.lua — Room actor implementation (单例, Game 进程, id=0)
--
-- 客户端业务消息经 Gate 注入发送方 player_id 后, 经 RPC_METHOD_FORWARD_ACTOR
-- 转发到本 Actor 的 on_rpc_forward:
--   wrapped = [8 字节小端 player_id][客户端 protobuf]
-- 发送方身份以 si_net_body_cid(wrapped) 还原, 负载以 si_net_body_payload(wrapped) 还原。
--
-- 房间列表存模块级内存表 (单例), 生产可换 CRecordMgr 持久化。
-- 开始战斗时: 生成 battle_id + auth_key, 把花名册/校验码写入 Redis,
-- 再经 ActorCreator.balanceCreate 跨进程创建 BattleActor 实例 (id=battle_id)。

local AT = xEngine.ActorType

-- 房间模块消息 ID (镜像 room.proto RoomMsgId)
local RoomMsgId = {
    CREATE_REQ   = 1, CREATE_RSP   = 2, LIST_REQ      = 3, LIST_RSP      = 4,
    JOIN_REQ     = 5, JOIN_RSP     = 6, LEAVE_REQ     = 7, LEAVE_RSP     = 8,
    READY_REQ    = 9, READY_RSP    = 10, KICK_REQ     = 11, KICK_RSP     = 12,
    TRANSFER_REQ = 13, START_REQ   = 14, START_RSP    = 15,
    INFO_NTF     = 16, MEMBER_NTF  = 17,
    BATTLE_END   = 18,  -- 内部消息 (battle.lua 战斗结束回房通知, 非客户端消息)
}

-- 战斗模式 / 可见性 (镜像 room.proto RoomMode / RoomVisibility)
local MODE = { NONE = 0, PVE = 1, PVP = 2 }
local VIS  = { NONE = 0, PUBLIC = 1, PRIVATE = 2, PASSWORD = 3 }

-- 错误码 (2001+ 为房间模块私有码)
local EC = {
    NONE               = 0,
    SESSION_INVALID    = 1001,
    INTERNAL           = 1999,
    ROOM_NOT_FOUND     = 2101,
    ROOM_FULL          = 2102,
    ROOM_WRONG_PASSWORD= 2103,
    ROOM_NOT_OWNER     = 2104,
    ROOM_NOT_ALL_READY = 2105,
    ROOM_ALREADY_IN    = 2106,
    ROOM_IN_BATTLE     = 2107,
}

-- 单例内存态 (存于 xEngine.room_state, 跨热更保留; 计数器放进可变表使自增写回持久)
-- isFresh: 新进程(xEngine.room_state 为 nil) vs 热更(内存态仍在, 不回读 Redis)
local isFresh = (xEngine.room_state == nil)
local hot = xEngine.room_state or { counters = {} }
xEngine.room_state = hot
if not hot.rooms then hot.rooms = {} end
if not hot.playerRoom then hot.playerRoom = {} end
local counters = hot.counters
-- 灰度更新后进程重启丢 module 内存: 从 Redis 回读持久化计数, 避免 roomId/battleId
-- 回退撞上旧战斗残留的 battle:* / battle:byroom / battle:byplayer 键 (客户端逻辑错误)。
-- Redis 未就绪时 Get 返回 "", tonumber 得 nil, 回退默认值 (与旧行为一致, 无副作用)。
if counters.nextRoomId == nil then
    local v = tonumber(xEngine.Redis.Get("room:counter:nextRoomId"))
    counters.nextRoomId = (v and v >= 1000) and v or 1000
end
if counters.nextBattleId == nil then
    local v = tonumber(xEngine.Redis.Get("room:counter:nextBattleId"))
    counters.nextBattleId = (v and v >= 5000) and v or 5000
end

local rooms      = hot.rooms       -- roomId -> { ownerId, mode, visibility, password, maxMembers, state, members={pid -> {name,ready,isOwner}} }
local playerRoom = hot.playerRoom  -- pid -> roomId

-- 房间内存态反序列化: "ownerId|mode|visibility|maxMembers|levelId|difficulty|state|battleId|members_str"
-- (字段 | 分隔, 成员 ; 分隔, 成员内 , 分隔; 角色名已排除 ,;|: 故安全; 密码不持久化)。
local function parse_room(s)
    local ownerId, mode, visibility, maxMembers, levelId, difficulty, state, battleId, membersStr =
        s:match("^(.-)|(.-)|(.-)|(.-)|(.-)|(.-)|(.-)|(.-)|(.*)$")
    if not ownerId then return nil end
    local room = {
        ownerId    = xEngine.Number.toInt(tonumber(ownerId)),
        mode       = xEngine.Number.toInt(tonumber(mode)),
        visibility = xEngine.Number.toInt(tonumber(visibility)),
        maxMembers = xEngine.Number.toInt(tonumber(maxMembers)),
        levelId    = xEngine.Number.toInt(tonumber(levelId)),
        difficulty = xEngine.Number.toInt(tonumber(difficulty)),
        state      = tonumber(state) or 0,
        members    = {},
    }
    local bid = tonumber(battleId)
    if bid and bid > 0 then room.battleId = bid end
    for m in (membersStr or ""):gmatch("[^;]+") do
        local pid, name, ready, isOwner = m:match("^(.-),(.-),(.-),(.-)$")
        if pid then
            room.members[xEngine.Number.toInt(tonumber(pid))] = {
                name    = name or "",
                ready   = (ready == "1"),
                isOwner = (isOwner == "1"),
            }
        end
    end
    return room
end

-- 灰度滚动 Game 进程后, 新进程从 Redis 回读重建 rooms/playerRoom, 否则大厅房间列表
-- 清空、玩家掉出房间 (客户端可见断裂)。热更时 isFresh=false 保留内存态不回读。
if isFresh then
    local restored = 0
    for idStr in (xEngine.Redis.SMembers("room:ids") or ""):gmatch("[^,]+") do
        if idStr ~= "" then
            local raw = xEngine.Redis.Get("room:data:" .. idStr)
            if raw and raw ~= "" then
                local room = parse_room(raw)
                local roomId = tonumber(idStr)
                if room and roomId then
                    rooms[roomId] = room
                    for pid in pairs(room.members) do playerRoom[pid] = roomId end
                    restored = restored + 1
                end
            end
        end
    end
    xEngine.Log.info("[Room] fresh reload: %d room(s) restored from Redis", restored)
end

local RoomClass = xEngine.Class.define("Room", xEngine.ActorBase)

function RoomClass:onInstall()
    xEngine.Log.info("[Room] onInstall")
    return true
end

function RoomClass:onUnInstall()
    xEngine.Log.info("[Room] onUnInstall")
    return true
end

function RoomClass:data_load(actor_data) return true end
function RoomClass:data_save(save_type) return true end
function RoomClass:onInit()    return true end
function RoomClass:onCreate()  return true end

--------------------------------------------------------------------------------
-- 辅助
--------------------------------------------------------------------------------
-- 玩家真实昵称 (player:{pid} 的 name 字段; 缺省回退占位 "player_<pid>")。
-- 角色名在建号时已校验排除 ,;|:%c (login.lua), 可安全嵌入逗号拼接的房间字符串。
local function player_name(pid)
    local attrs = xEngine.Role.parseAttrs(pid)
    return (attrs.name and attrs.name ~= "") and attrs.name or ("player_" .. xEngine.Number.intKey(pid))
end

function RoomClass:count_members(room)
    local n = 0
    for _ in pairs(room.members) do n = n + 1 end
    return n
end

function RoomClass:members_str(room)
    local parts = {}
    for pid, m in pairs(room.members) do
        table.insert(parts, table.concat({
            xEngine.Number.intKey(pid), m.name,
            m.ready and "1" or "0", m.isOwner and "1" or "0",
        }, ","))
    end
    return table.concat(parts, ";")
end

-- 房间持久化: 序列化 "ownerId|mode|visibility|maxMembers|levelId|difficulty|state|battleId|members_str"
-- (密码不持久化: 无字符校验可能含定界符, 且为 VIS.PASSWORD 边角场景)。
function RoomClass:serialize_room(room)
    return table.concat({
        xEngine.Number.intKey(room.ownerId),
        xEngine.Number.intKey(room.mode),
        xEngine.Number.intKey(room.visibility),
        xEngine.Number.intKey(room.maxMembers),
        xEngine.Number.intKey(room.levelId),
        xEngine.Number.intKey(room.difficulty),
        tostring(room.state or 0),
        xEngine.Number.intKey(room.battleId or 0),
        self:members_str(room),
    }, "|")
end

-- 落 Redis: room:data:{id} = 序列化串; room:ids = 房间 id SET (新进程回读枚举用)。
function RoomClass:save_room(roomId)
    local room = rooms[roomId]
    if not room then return end
    local idStr = xEngine.Number.intKey(roomId)
    xEngine.Redis.Set("room:data:" .. idStr, self:serialize_room(room))
    xEngine.Redis.SAdd("room:ids", idStr)
end

function RoomClass:remove_room(roomId)
    local idStr = xEngine.Number.intKey(roomId)
    xEngine.Redis.Del("room:data:" .. idStr)
    xEngine.Redis.SRem("room:ids", idStr)
end

function RoomClass:room_str(roomId, room)
    local owner = room.members[room.ownerId]
    local ownerName = owner and owner.name or ("player_" .. xEngine.Number.intKey(room.ownerId))
    local name = ownerName .. "的房间"
    if xEngine.Number.toInt(room.state) == 1 then name = name .. "(战斗中)" end
    return table.concat({
        xEngine.Number.intKey(roomId),
        xEngine.Number.intKey(room.ownerId),
        xEngine.Number.intKey(room.mode),
        xEngine.Number.intKey(room.visibility),
        tostring(self:count_members(room)),
        xEngine.Number.intKey(room.maxMembers),
        tostring(room.state),
        name,
    }, ",")
end

-- 组装非私密房间列表字符串 (复用 handle_list 的过滤逻辑)
function RoomClass:list_str()
    local parts = {}
    for roomId, room in pairs(rooms) do
        if xEngine.Number.toInt(room.visibility) ~= VIS.PRIVATE then
            table.insert(parts, self:room_str(roomId, room))
        end
    end
    return table.concat(parts, ";")
end

-- 房间列表实时广播给所有客户端 (playerId=0 哨兵 => Gate 遍历全部在线连接下发,
-- 复用 ROOM_LIST_RSP 负载, 客户端 onRoomListRsp 收到后即时刷新房间列表控件)。
function RoomClass:broadcast_list()
    luacore.si_push_rpc(0, luacore_room.si_pb_room_list_rsp(EC.NONE, self:list_str()))
end

-- 成员列表变更实时推送给房间内所有成员 (ROOM_MEMBER_NTF)。
function RoomClass:broadcast_members(roomId, room)
    local ntf = luacore_room.si_pb_room_member_ntf(roomId, self:members_str(room))
    for pid in pairs(room.members) do
        luacore.si_push_rpc(pid, ntf)
    end
end

-- 成员/房主变化后的统一同步: 广播房间列表 (人数变化给所有客户端) + 推成员 NTF (房内成员刷新)。
function RoomClass:notify_room_change(roomId, room)
    self:broadcast_list()
    self:broadcast_members(roomId, room)
end

-- 从房间移除成员; 空房销毁, 房主离开则转让给首个剩余成员
function RoomClass:do_leave(roomId, pid)
    local room = rooms[roomId]
    if not room or not room.members[pid] then return end

    local wasOwner = room.members[pid].isOwner
    room.members[pid] = nil
    playerRoom[pid] = nil

    if self:count_members(room) == 0 then
        rooms[roomId] = nil
        self:remove_room(roomId)   -- 空房即删, 同步清 Redis
        self:broadcast_list()   -- 空房即删, 实时广播房间列表 (客户端即时刷掉该房)
        return
    end
    if wasOwner then
        for otherPid, m in pairs(room.members) do
            m.isOwner = true
            room.ownerId = otherPid
            break
        end
    end
    self:save_room(roomId)   -- 成员/房主变化落库
    self:notify_room_change(roomId, room)   -- 人数/房主变化同步
end

--------------------------------------------------------------------------------
-- 业务消息总入口
--------------------------------------------------------------------------------
function RoomClass:on_rpc_forward(wrapped)
    local senderId  = xEngine.Number.toInt(luacore.si_net_body_cid(wrapped))
    local payload   = luacore.si_net_body_payload(wrapped)
    local smallType = luacore.si_pb_small_type(payload)

    if smallType == RoomMsgId.CREATE_REQ then
        return self:handle_create(senderId, payload)
    elseif smallType == RoomMsgId.LIST_REQ then
        return self:handle_list(senderId)
    elseif smallType == RoomMsgId.JOIN_REQ then
        return self:handle_join(senderId, payload)
    elseif smallType == RoomMsgId.LEAVE_REQ then
        return self:handle_leave(senderId, payload)
    elseif smallType == RoomMsgId.READY_REQ then
        return self:handle_ready(senderId, payload)
    elseif smallType == RoomMsgId.KICK_REQ then
        return self:handle_kick(senderId, payload)
    elseif smallType == RoomMsgId.TRANSFER_REQ then
        return self:handle_transfer(senderId, payload)
    elseif smallType == RoomMsgId.START_REQ then
        return self:handle_start(senderId, payload)
    elseif smallType == RoomMsgId.BATTLE_END then
        return self:handle_battle_end(senderId)   -- senderId 即 battle_id
    end

    xEngine.Log.warn("[Room] unknown small_type=%d", smallType)
    return ""
end

function RoomClass:handle_create(senderId, payload)
    local mode       = xEngine.Number.toInt(luacore.si_pb_varint_field(payload, 3))
    local visibility = xEngine.Number.toInt(luacore.si_pb_varint_field(payload, 4))
    local password   = luacore.si_pb_string_field(payload, 5)
    local maxMembers = xEngine.Number.toInt(luacore.si_pb_varint_field(payload, 6))
    if maxMembers < 1 then maxMembers = 1 end
    if maxMembers > 5 then maxMembers = 5 end

    -- C6 S6: 房主选定关卡/难度随 CREATE_REQ 流入 (匹配流源头), 校验后存房间态。
    -- levelId 11..68 (48 关); difficulty 0/1/2。缺省/越界回退 11/0。
    local levelId    = xEngine.Number.toInt(luacore.si_pb_varint_field(payload, 7))
    local difficulty = xEngine.Number.toInt(luacore.si_pb_varint_field(payload, 8))
    if levelId < 11 or levelId > 68 then levelId = 11 end
    if difficulty < 0 or difficulty > 2 then difficulty = 0 end

    -- 已在房间的先退 (房间保留: 战斗结束回房后房主重建新房时, 统一走 do_leave 离开旧房)
    if playerRoom[senderId] then
        self:do_leave(playerRoom[senderId], senderId)
    end

    local roomId = counters.nextRoomId
    counters.nextRoomId = counters.nextRoomId + 1
    xEngine.Redis.Set("room:counter:nextRoomId", xEngine.Number.intKey(counters.nextRoomId))
    local room = {
        ownerId    = senderId,
        mode       = mode,
        visibility = visibility,
        password   = password,
        maxMembers = maxMembers,
        levelId    = levelId,
        difficulty = difficulty,
        state      = 0,
        members    = {},
    }
    room.members[senderId] = { name = player_name(senderId), ready = false, isOwner = true }
    rooms[roomId] = room
    playerRoom[senderId] = roomId
    self:save_room(roomId)   -- 新房落库, 滚动后大厅列表/成员关系保留

    xEngine.Log.info("[Room] create: room=%d owner=%d mode=%d vis=%d", roomId, senderId, mode, visibility)
    self:broadcast_list()   -- 建房实时广播房间列表给所有客户端 (实时刷新房间列表)
    return luacore_room.si_pb_room_create_rsp(EC.NONE, roomId, self:members_str(room))
end

function RoomClass:handle_list(senderId)
    return luacore_room.si_pb_room_list_rsp(EC.NONE, self:list_str())
end

function RoomClass:handle_join(senderId, payload)
    local roomId   = xEngine.Number.toInt(luacore.si_pb_varint_field(payload, 3))
    local password = luacore.si_pb_string_field(payload, 4)
    local room = rooms[roomId]
    if not room then return luacore_room.si_pb_room_join_rsp(EC.ROOM_NOT_FOUND, roomId, "") end

    -- 幂等: 自己已在本房 (战斗结束回房后客户端重新 join 刷新「我的房间」成员),
    -- 回当前成员列表而非报 ROOM_ALREADY_IN。
    if room.members[senderId] then
        return luacore_room.si_pb_room_join_rsp(EC.NONE, roomId, self:members_str(room))
    end

    -- 房间保留后, 列表中会出现「战斗中」房间: 禁止新玩家加入进行中的战斗房。
    if xEngine.Number.toInt(room.state) == 1 then
        return luacore_room.si_pb_room_join_rsp(EC.ROOM_IN_BATTLE, roomId, "")
    end

    if xEngine.Number.toInt(room.visibility) == VIS.PASSWORD and password ~= room.password then
        return luacore_room.si_pb_room_join_rsp(EC.ROOM_WRONG_PASSWORD, roomId, "")
    end
    if self:count_members(room) >= room.maxMembers then
        return luacore_room.si_pb_room_join_rsp(EC.ROOM_FULL, roomId, "")
    end

    -- 已在其它房间的先退: do_leave 会把空房整房销毁并广播房间列表,
    -- 避免玩家同时挂在两个房间、旧房残留 (从自己房间 join 他人房间时旧房应删除)。
    if playerRoom[senderId] and playerRoom[senderId] ~= roomId then
        self:do_leave(playerRoom[senderId], senderId)
    end

    room.members[senderId] = { name = player_name(senderId), ready = false, isOwner = false }
    playerRoom[senderId] = roomId
    self:save_room(roomId)   -- 新成员落库
    xEngine.Log.info("[Room] join: room=%d player=%d", roomId, senderId)
    self:notify_room_change(roomId, room)   -- 人数变化: 广播列表 + 推成员 NTF
    return luacore_room.si_pb_room_join_rsp(EC.NONE, roomId, self:members_str(room))
end

function RoomClass:handle_leave(senderId, payload)
    local roomId = xEngine.Number.toInt(luacore.si_pb_varint_field(payload, 3))
    -- 断线兜底: 客户端断线时由 Gate 合成 roomId 缺省(=0) 的 LEAVE_REQ, 此时按服务器
    -- 权威的 playerRoom 映射反查房间 (断线离开不依赖客户端自报房间号)。
    if roomId <= 0 then
        roomId = playerRoom[senderId]
    end
    if not roomId or not rooms[roomId] then
        return luacore_room.si_pb_room_leave_rsp(EC.ROOM_NOT_FOUND, roomId or 0)
    end
    self:do_leave(roomId, senderId)
    xEngine.Log.info("[Room] leave: room=%d player=%d", roomId, senderId)
    return luacore_room.si_pb_room_leave_rsp(EC.NONE, roomId)
end

function RoomClass:handle_ready(senderId, payload)
    local roomId = xEngine.Number.toInt(luacore.si_pb_varint_field(payload, 3))
    local ready  = luacore.si_pb_bool_field(payload, 4)
    local room = rooms[roomId]
    if not room or not room.members[senderId] then
        return luacore_room.si_pb_room_ready_rsp(EC.ROOM_NOT_FOUND, roomId)
    end
    room.members[senderId].ready = ready
    self:save_room(roomId)   -- 准备状态落库
    self:broadcast_members(roomId, room)   -- 准备状态变化推给房内成员
    return luacore_room.si_pb_room_ready_rsp(EC.NONE, roomId)
end

function RoomClass:handle_kick(senderId, payload)
    local roomId   = xEngine.Number.toInt(luacore.si_pb_varint_field(payload, 3))
    local targetId = xEngine.Number.toInt(luacore.si_pb_varint_field(payload, 4))
    local room = rooms[roomId]
    if not room then return luacore_room.si_pb_room_kick_rsp(EC.ROOM_NOT_FOUND, roomId) end
    if room.ownerId ~= senderId then return luacore_room.si_pb_room_kick_rsp(EC.ROOM_NOT_OWNER, roomId) end
    if targetId == senderId then return luacore_room.si_pb_room_kick_rsp(EC.INTERNAL, roomId) end
    if not room.members[targetId] then return luacore_room.si_pb_room_kick_rsp(EC.ROOM_NOT_FOUND, roomId) end

    room.members[targetId] = nil
    playerRoom[targetId] = nil
    self:save_room(roomId)   -- 踢人后落库
    xEngine.Log.info("[Room] kick: room=%d target=%d", roomId, targetId)
    self:notify_room_change(roomId, room)   -- 人数变化: 广播列表 + 推成员 NTF
    return luacore_room.si_pb_room_kick_rsp(EC.NONE, roomId)
end

function RoomClass:handle_transfer(senderId, payload)
    local roomId     = xEngine.Number.toInt(luacore.si_pb_varint_field(payload, 3))
    local newOwnerId = xEngine.Number.toInt(luacore.si_pb_varint_field(payload, 4))
    local room = rooms[roomId]
    if not room or room.ownerId ~= senderId then return "" end
    local target = room.members[newOwnerId]
    if not target then return "" end

    if room.members[senderId] then room.members[senderId].isOwner = false end
    target.isOwner = true
    room.ownerId = newOwnerId
    self:save_room(roomId)   -- 房主转让落库
    xEngine.Log.info("[Room] transfer: room=%d new_owner=%d", roomId, newOwnerId)
    self:broadcast_members(roomId, room)   -- 房主变化推给房内成员
    return ""   -- TRANSFER_REQ 无回包
end

function RoomClass:handle_start(senderId, payload)
    local roomId = xEngine.Number.toInt(luacore.si_pb_varint_field(payload, 3))
    local room = rooms[roomId]
    if not room then return luacore_room.si_pb_room_start_rsp(EC.ROOM_NOT_FOUND, roomId, 0, "") end
    if room.ownerId ~= senderId then return luacore_room.si_pb_room_start_rsp(EC.ROOM_NOT_OWNER, roomId, 0, "") end

    for pid, m in pairs(room.members) do
        if not m.ready then
            return luacore_room.si_pb_room_start_rsp(EC.ROOM_NOT_ALL_READY, roomId, 0, "")
        end
    end

    local battleId = counters.nextBattleId
    counters.nextBattleId = counters.nextBattleId + 1
    xEngine.Redis.Set("room:counter:nextBattleId", xEngine.Number.intKey(counters.nextBattleId))
    local authKey = "bk" .. xEngine.Number.intKey(battleId) .. "-" .. tostring(luacore.si_rand_between(100000, 999999))

    -- 写 Redis: 战斗房花名册 (逗号分隔 pid) + 进入校验 auth_key + 关卡配置 (levelId,difficulty)
    local pids = {}
    for pid, _ in pairs(room.members) do table.insert(pids, xEngine.Number.intKey(pid)) end
    xEngine.Redis.Set("battle:roster:" .. xEngine.Number.intKey(battleId), table.concat(pids, ","))
    xEngine.Redis.Set("battle:auth:" .. xEngine.Number.intKey(battleId), authKey)
    xEngine.Redis.Set("battle:level:" .. xEngine.Number.intKey(battleId),
        tostring(room.levelId) .. "," .. tostring(room.difficulty))

    -- 玩家装备 (服务端权威, 由 Player actor 上传持久化在 player:equip:{pid}):
    -- 每成员读回并组装 battle:equip:{battleId} = "pid:primaryGun:g0,...,g5;..." (无装备回退 0 + 全空载具)。
    local equipParts = {}
    for pid, _ in pairs(room.members) do
        local raw = xEngine.Redis.Get("player:equip:" .. xEngine.Number.intKey(pid))
        local primaryGun, equip = raw:match("^(%d+)|(.+)$")
        if not primaryGun then primaryGun = "0" end
        if not equip then equip = "22,22,22,22,22,22" end
        equipParts[#equipParts + 1] = table.concat({
            xEngine.Number.intKey(pid), primaryGun, equip,
        }, ":")
    end
    xEngine.Redis.Set("battle:equip:" .. xEngine.Number.intKey(battleId), table.concat(equipParts, ";"))

    -- 跨进程创建 BattleActor 实例 (id=battle_id), 负载均衡到 Battle 进程
    local actor = xEngine.ActorCreator.balanceCreate(AT.BATTLE, battleId)
    if not actor then
        xEngine.Log.error("[Room] start: create battle failed battleId=%d", battleId)
        return luacore_room.si_pb_room_start_rsp(EC.INTERNAL, roomId, 0, "")
    end

    -- 战斗进程直连地址: 客户端拿到 ip:port + authKey + battleId + PlayerID 直连 Battle 进程
    local battleHost = actor._host_ip or ""
    local battlePort = actor._listen_port or 0
    -- 观战者直连同样走 Battle 进程: 写入 battle:host:{battleId} = "ip:listen_port",
    -- 由 SpectatorActor handle_enter 读出回给观战客户端 (战斗结束 battle.lua 清理)。
    xEngine.Redis.Set("battle:host:" .. xEngine.Number.intKey(battleId),
        battleHost .. ":" .. tostring(battlePort))

    -- 观战实例 (SpectatorActor) 不再开局即建: 改为惰性创建 —— 仅在客户端首次发起观战
    -- 请求时由 Gate 创建 (见 gate.lua ensure_spectator), 避免每场战斗都常驻一个空转的
    -- 观战 Actor。战斗结束时 BattleActor finalize 仍会全局删除对应 SpectatorActor。

    -- 观战按「房间列表 ID / 玩家 ID」反查 live battle_id 的映射 (Gate 进程解析观战目标用;
    -- 战斗结束由 battle.lua on_battle_finalize_msg 清理, 避免残留指向已死战斗房)。
    --   battle:byroom:{roomId}   = battleId   (房列表 ID → 战斗房)
    --   battle:byplayer:{pid}    = battleId   (参战玩家 → 战斗房)
    --   battle:room:{battleId}   = roomId     (反向索引, 供结束清理 battle:byroom)
    xEngine.Redis.Set("battle:byroom:" .. xEngine.Number.intKey(roomId), xEngine.Number.intKey(battleId))
    xEngine.Redis.Set("battle:room:" .. xEngine.Number.intKey(battleId), xEngine.Number.intKey(roomId))
    for pid, _ in pairs(room.members) do
        xEngine.Redis.Set("battle:byplayer:" .. xEngine.Number.intKey(pid), xEngine.Number.intKey(battleId))
    end

    room.state = 1
    room.battleId = battleId
    self:save_room(roomId)   -- 开局状态落库 (state=1,battleId)
    xEngine.Log.info("[Room] start: room=%d battle=%d auth=%s", roomId, battleId, authKey)

    -- 开局广播: 房主经 START_RSP 直接回包获知, 其余成员经推送链路获知 (START_NTF)。
    -- 复用 start_rsp 负载 (battle_id + auth_key), 客户端按 ROOM_START_RSP 路由进入战斗。
    for pid in pairs(room.members) do
        if pid ~= senderId then
            luacore.si_push_rpc(pid,
                luacore_room.si_pb_room_start_rsp(EC.NONE, roomId, battleId, authKey, battleHost, battlePort))
        end
    end

    -- 房间保留: 开局后房间仍留在列表 (名称带「(战斗中)」), 供观战/识别;
    -- playerRoom 映射也保留, 战斗结束由 battle.lua 通知回房 (state 回 0, 全员回组队房)。
    self:broadcast_list()

    return luacore_room.si_pb_room_start_rsp(EC.NONE, roomId, battleId, authKey, battleHost, battlePort)
end

-- 战斗结束回房 (battle.lua finish_battle 经 si_forward_rpc 通知): 房间保留、状态回等待、
-- 全员 ready 复位, 重新广播列表 + 成员。senderId 即 battle_id, 经 battle:room:{battleId} 反查房间。
function RoomClass:handle_battle_end(battleId)
    local roomIdStr = xEngine.Redis.Get("battle:room:" .. xEngine.Number.intKey(battleId))
    local roomId = roomIdStr and xEngine.Number.toInt(tonumber(roomIdStr))
    local room = roomId and rooms[roomId]
    if not room then
        xEngine.Log.info("[Room] battle end: room not found battle=%d", battleId)
        return ""
    end

    -- req4: 战斗结束若组队房已无人 (全员战斗中离开/断线被清), 直接删除组队房, 不再回「等待」状态。
    if self:count_members(room) == 0 then
        rooms[roomId] = nil
        self:remove_room(roomId)   -- 空房即删, 同步清 Redis
        xEngine.Log.info("[Room] battle end: empty room deleted room=%d battle=%d", roomId, battleId)
        self:broadcast_list()
        return ""
    end

    room.state = 0
    room.battleId = nil
    for _, m in pairs(room.members) do m.ready = false end
    self:save_room(roomId)   -- 回等待态落库 (state=0, 全员 ready 复位)
    xEngine.Log.info("[Room] battle end: room=%d battle=%d -> waiting", roomId, battleId)
    self:notify_room_change(roomId, room)   -- 广播列表 (名称回无「战斗中」) + 推成员 NTF
    return ""
end

xEngine.ActorMgr.registerPrototype(AT.ROOM, RoomClass)
xEngine.ActorMgr.install(AT.ROOM, 0)
