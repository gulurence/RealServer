-- battle.lua — Battle actor implementation (多实例, Battle 进程, id=battle_id)
--
-- 每个战斗房对应一个 BattleActor 实例 (id == battle_id)。跨进程创建由 Room 在
-- 开始时经 ActorCreator.balanceCreate(AT.BATTLE, battleId) 触发; 本 Actor 的
-- CLuaActor::Create 会为每个实例生成独立 Lua 表 (ctor 初始化 per-instance 状态)。
--
-- 帧同步核心 (battlecore) 是确定性纯 C++ 模块, 通过 si_battlecore_* 绑定:
--   handle = si_battlecore_new()  (进程内注册表小整数 id, 规避指针→double 丢精度)
--   种子: 由 battle_id 派生, 保证客户端/服务器一致。
--
-- 说明: FRAME_NTF / END_NTF 是服务器主动推送 (需 Gate→客户端 push 链路),
-- 当前骨架未接推送, 仅在本地推进帧并落日志; 客户端回包走 on_rpc_forward 返回值。

local AT = xEngine.ActorType

-- 战斗模块消息 ID (镜像 battle.proto BattleMsgId)
local BattleMsgId = {
    ENTER_REQ     = 1, ENTER_RSP     = 2, START_NTF    = 3, INPUT_REQ     = 4,
    FRAME_NTF     = 5, SNAPSHOT_NTF  = 6, END_NTF      = 7, LEAVE_REQ     = 8,
    LEAVE_RSP     = 9, HASH_NTF      = 10,
    DISCONNECT    = 11,  -- 内部消息 (Gate 断线收尾转发, 非客户端消息; 不入 proto)
    SPECTATOR_ENTER = 12, -- 内部消息 (CNetBattleGateway 观战登录成功后投递, 触发下推快照; 不入 proto)
}

-- 错误码 (2001+ 为战斗模块私有码)
local EC = {
    NONE               = 0,
    SESSION_INVALID    = 1001,
    INTERNAL           = 1999,
    BATTLE_AUTH_FAILED = 2301,
}

-- 战斗配置常量 (默认值, 见 CLAUDE.md 战斗补齐说明)
local kBattleTimeLimitSec  = 300      -- 战斗时长上限 (秒, 5 分钟); 超时=失败
local kBattleCleanupDelayMs = 60000   -- 结束/全离开后延迟卸载时长 (毫秒, 用户第 9 条)
local kGoldEntryFee        = 10       -- 金币入场费 (胜负都扣, 结算时 gold-=cost 钳位 0)
local kParticipateExp      = 20       -- 基础参与经验 (胜方)
local kFrameRate           = 20       -- 20Hz 逻辑帧 (帧/秒)
local kMaxTotalDeaths        = 10      -- 全员累计死亡次数上限 (>10 触发结算, req3)
local kSnapshotIntervalFrames= 1200    -- 周期快照间隔 (帧; 60s × 20Hz, req5)
local kReconnectWindowSec    = 90      -- 断线重连窗口 (秒; 超时按离开处理, req8)
-- 击杀排名奖励表 (按名次降序; 超出表尾只给基础参与经验)
local RewardTier = {
    { exp = 100, gold = 50 },   -- 第 1 名
    { exp = 80,  gold = 40 },   -- 第 2 名
    { exp = 60,  gold = 30 },   -- 第 3 名
}

-- 战斗 → Room 的内部通知 (镜像 room.lua RoomMsgId): 战斗结束回房 / 中途退出离开房间。
local ROOM_SMALL = { LEAVE_REQ = 7, BATTLE_END = 18 }

-- 编码仅含 small_type(field 2, varint) 的最小 protobuf 负载: tag = (2<<3)|0 = 0x10。
-- (与 gate.lua 断线收尾同款; Room 的 on_rpc_forward 只读 si_pb_small_type。)
local function encode_small_type(smallType)
    return string.char(0x10, smallType % 0x100)
end

local BattleClass = xEngine.Class.define("Battle", xEngine.ActorBase)

--------------------------------------------------------------------------------
-- 构造: 每个实例独立状态
--------------------------------------------------------------------------------
function BattleClass:ctor(actor_type, actor_id_or_name, actor_type_name)
    self:super("ctor", actor_type, actor_id_or_name, actor_type_name)
    self.players     = {}   -- pid -> { name, spawnIndex }
    self.rosterOrder = {}   -- pid 有序列表 (花名册顺序, 保证两端输入投递顺序一致)
    self.entered     = {}   -- pid -> true (已进入)
    self.core        = nil  -- battlecore 句柄
    self.seed        = 0
    self.levelId     = 11   -- 本场关卡 (C6 S6: 从 battle:level Redis 读, 缺省 11)
    self.difficulty  = 0    -- 本场难度 (缺省 0)
    self.frame       = 0
    self.finished    = false
    self.startNtfSent = false   -- 是否已广播 START_NTF (全员就绪只发一次)
    self.frameInputs = {}   -- clientFrame -> { pid = {moveDir, shootDir, skillId, fire} }
    self.frameReady  = {}   -- clientFrame -> 已收输入数
    self.frameHash   = {}   -- frame -> 该帧 tick 后的核心哈希 (handle_hash 比对已确认帧, 规避时序错位)
    self.replayFrames = {}  -- 逐帧输入录制 (战斗回放): { {pid, move, shoot, skill, fire}, ... } 按帧

    -- 战斗时长 / 结束 / 生命周期
    self.startTs          = 0    -- 战斗开始时间戳 (unix 秒, ensure_core 时记录)
    self.duration         = 0    -- 战斗时长 (秒, finish_battle 时结算)
    self.endReason        = nil  -- 结束原因: "win" / "timeout" / "all_left"
    self.finalizeScheduled = false   -- BATTLE_FINALIZE 是否已调度 (防重复)
    self.kills            = {}   -- pid -> 击杀数 (结算/战报用)
    self.settleResult     = {}   -- pid -> {rank=, exp=, gold=, kills=} (战报 stats 用)

    -- 断线重连 (req2/6/8)
    self.online           = {}   -- pid -> true (已连接且在战斗)
    self.disconnectTs     = {}   -- pid -> 断线墙钟秒 (si_time_cur_sec; 90s 窗口判定)
    self.lastSnapshot     = nil  -- 最近一次周期快照 { frame=, state= } (req5)
    self.reconnectFrames  = {}   -- 快照帧之后缓存的逐帧输入 { {frameId=, inputsStr=}, ... } (req6)

    -- 观战者 (Battle 进程侧缓存; 权威名单在 Redis battle:spectator:{battleId})
    self.spectators         = {}   -- pid -> true
    self.spectatorCacheFrame = -1  -- 上次刷新观战名单的帧号
end

function BattleClass:onInstall()
    xEngine.Log.info("[Battle] onInstall id=%d", self._actor_id)
    return true
end

function BattleClass:onUnInstall()
    if self.core then
        luacore_battle.si_battlecore_free(self.core)
        self.core = nil
    end
    xEngine.Log.info("[Battle] onUnInstall id=%d", self._actor_id)
    return true
end

function BattleClass:data_load(actor_data)
    local bid = xEngine.Number.intKey(self._actor_id)
    local meta = xEngine.Redis.Get("battle:migrate:" .. bid)
    if meta == nil or meta == "" then
        return true   -- 全新战斗 (无迁移快照): 走现有 handle_enter → ensure_core 流程
    end

    -- 花名册/装备/关卡本就是 Redis 权威 (battle:roster / battle:equip / battle:level),
    -- ensure_core 会从 Redis 重建 self.players/self.rosterOrder 并初始化核心 (seed 由
    -- battleId 派生, 与旧进程一致)。
    self:ensure_core()

    -- 反序列化 C++ 核心模拟状态 (权威帧状态: 波次/英雄位置/冷却等)。
    local coreState = xEngine.Redis.Get("battle:migrate:core:" .. bid)
    if coreState ~= "" and self.core then
        if not luacore_battle.si_battlecore_deserialize(self.core, coreState) then
            xEngine.Log.error("[Battle] migrate: core deserialize failed battle=%d", self._actor_id)
        end
    end

    -- 元数据: frame|startTs|finished|endReason|startNtfSent|kills(pid:count,...)
    local fields = {}
    for v in (meta .. "|"):gmatch("([^|]*)|") do fields[#fields + 1] = v end
    self.frame        = tonumber(fields[1]) or 0
    self.startTs      = tonumber(fields[2]) or luacore.si_time_cur_sec()
    self.finished     = fields[3] == "1"
    self.endReason    = (fields[4] ~= "" and fields[4]) or nil
    self.startNtfSent = fields[5] == "1"

    self.kills = {}
    for rec in (((fields[6] or "") .. ",")):gmatch("([^,]*),") do
        local pidStr, cntStr = rec:match("^(%d+):(%d+)$")
        if pidStr then self.kills[tonumber(pidStr)] = tonumber(cntStr) end
    end

    -- 迁移后全员直连随旧进程消亡: 标记离线并重置 90s 重连窗口, 由
    -- check_reconnect_timeouts 兜底 (超时按离开处理); 客户端断线回组队房后可重进。
    for _, pid in ipairs(self.rosterOrder) do
        self.entered[pid] = true
        self.online[pid] = false
        self.disconnectTs[pid] = luacore.si_time_cur_sec()
    end

    xEngine.Log.info("[Battle] migrate: restored battle=%d frame=%d players=%d",
        self._actor_id, self.frame, #self.rosterOrder)
    return true
end

function BattleClass:data_save(save_type)
    local bid = xEngine.Number.intKey(self._actor_id)
    if save_type == 3 then
        -- 战斗房卸载/回收: 清迁移快照 (含二进制核心状态)
        xEngine.Redis.Del("battle:migrate:" .. bid)
        xEngine.Redis.Del("battle:migrate:core:" .. bid)
        return true
    end

    if not self.core then return true end   -- 尚未初始化: 无状态可迁 (走正常开局)

    -- 核心模拟状态 (二进制, 独立 key 避免定界符冲突)
    xEngine.Redis.Set("battle:migrate:core:" .. bid,
        luacore_battle.si_battlecore_serialize(self.core))

    -- 元数据 (文本, | 定界; 字段值均不含 | 或 ,)
    local kills = {}
    for pid, cnt in pairs(self.kills or {}) do
        kills[#kills + 1] = xEngine.Number.intKey(pid) .. ":" .. tostring(cnt)
    end
    local meta = table.concat({
        tostring(self.frame),
        tostring(self.startTs),
        self.finished and "1" or "0",
        self.endReason or "",
        self.startNtfSent and "1" or "0",
        table.concat(kills, ","),
    }, "|")
    xEngine.Redis.Set("battle:migrate:" .. bid, meta)

    xEngine.Log.info("[Battle] migrate: saved battle=%d frame=%d", self._actor_id, self.frame)
    return true
end

function BattleClass:onInit()    return true end
function BattleClass:onCreate()  return true end

--------------------------------------------------------------------------------
-- 辅助
--------------------------------------------------------------------------------
function BattleClass:players_str()
    -- 按花名册顺序 (rosterOrder) 而非 pairs 迭代, 保证两端玩家顺序一致 (帧同步确定性)。
    -- 每条 "pid,name,spawnIndex,primaryGun,level,exp,gold,attack,defense,hp,g0,g1,g2,g3,g4,g5"
    -- (属性 + 装备随花名册下发, C++ 侧 si_pb_battle_enter_rsp 按字段位置解析)。
    local parts = {}
    for _, pid in ipairs(self.rosterOrder) do
        local p = self.players[pid]
        if p then
            table.insert(parts, table.concat({
                xEngine.Number.intKey(pid), p.name, tostring(p.spawnIndex),
                tostring(p.primaryGun or 0), tostring(p.level or 1),
                tostring(p.exp or 0), tostring(p.gold or 0),
                tostring(p.attack or 10), tostring(p.defense or 0), tostring(p.hp or 100),
                p.equip or "22,22,22,22,22,22",
            }, ","))
        end
    end
    return table.concat(parts, ";")
end

-- 首次进入时初始化战斗核心: 派生种子 + 读花名册 (Room start 写入 Redis)
function BattleClass:ensure_core()
    if self.core then return end

    local battleId = self._actor_id
    self.seed = (xEngine.Number.toInt(battleId) % 0xFFFFFFFF) + 1

    local roster = xEngine.Redis.Get("battle:roster:" .. xEngine.Number.intKey(battleId))
    if roster == "" then roster = "0" end

    self.core = luacore_battle.si_battlecore_new()
    if not luacore_battle.si_battlecore_init(self.core, self.seed, roster) then
        xEngine.Log.error("[Battle] core init failed battle=%d", battleId)
    end
    -- C6 S6: 关卡/难度从匹配流流入 —— Room start 已写入 battle:level:{battleId}="levelId,difficulty",
    -- 这里读回并注入 (缺省回退 11/0, 兼容旧房间)。与客户端共用 LevelDataLoader, 必须在首次 tick 前调用。
    local lvlStr = xEngine.Redis.Get("battle:level:" .. xEngine.Number.intKey(battleId))
    local lvlId, lvlDiff = lvlStr:match("^(%d+),(%d+)$")
    if lvlId then
        self.levelId    = tonumber(lvlId)
        self.difficulty = tonumber(lvlDiff)
    end
    if not luacore_battle.si_battlecore_setup_level(self.core, self.levelId, self.difficulty) then
        xEngine.Log.error("[Battle] level data inject failed battle=%d level=%d diff=%d",
            battleId, self.levelId, self.difficulty)
    end

    -- 玩家装备 (局外系统经 Room 写入 battle:equip:{battleId} = "pid:primaryGun:g0,...,g5;..."):
    -- 读回解析成 pid → {primaryGun, equip}; 首次 tick 前用 SetHeroGun 注入初始主枪 (与客户端同值)。
    local equipStr = xEngine.Redis.Get("battle:equip:" .. xEngine.Number.intKey(battleId))
    local equipMap = {}
    for rec in (equipStr .. ";"):gmatch("([^;]*);") do
        local pidStr, gunStr, guns = rec:match("^(%d+):(%d+):(.+)$")
        if pidStr and gunStr then
            equipMap[tonumber(pidStr)] = { tonumber(gunStr), guns or "22,22,22,22,22,22" }
        end
    end

    local idx = 0
    for pidStr in (roster .. ","):gmatch("([^,]*),") do
        if pidStr ~= "" then
            local pid = tonumber(pidStr)
            local eq = equipMap[pid]
            -- 真实属性 (基础=昵称/等级/经验/金币, 战斗=血量/攻击/防御): 角色热数据
            -- player:{pid} = "name|level|job|exp|gold|attack|defense|hp" (带入战斗房间展示,
            -- 非确定性状态; 缺省回退占位昵称/等级 1 / 攻击 10 / 防御 0 / 血量 100)。
            local attrs = xEngine.Role.parseAttrs(pid)
            self.players[pid] = {
                name       = (attrs.name and attrs.name ~= "") and attrs.name or ("player_" .. pidStr),
                level      = attrs.level or 1,
                exp        = attrs.exp or 0,
                gold       = attrs.gold or 0,
                attack     = attrs.attack or 10,
                defense    = attrs.defense or 0,
                hp         = attrs.hp or 100,
                spawnIndex = idx,
                primaryGun = eq and eq[1] or 0,
                equip      = eq and eq[2] or "22,22,22,22,22,22",
            }
            self.rosterOrder[#self.rosterOrder + 1] = pid   -- 记录有序花名册
            -- 初始主枪注入 (必须 init + setup_level 之后、首次 tick 之前; 服务器与客户端同值)
            luacore_battle.si_battlecore_set_gun(self.core, pid, self.players[pid].primaryGun)
            idx = idx + 1
        end
    end
    self.startTs = luacore.si_time_cur_sec()   -- 战斗开始时间戳 (战斗时长/战报用)
    xEngine.Log.info("[Battle] core init battle=%d seed=%d players=%d level=%d diff=%d",
        battleId, self.seed, idx, self.levelId, self.difficulty)
end

-- 在线 (已连接且在战斗) 玩家数: 锁步输入收集只等在线玩家, 离线玩家补空输入不阻塞帧推进。
function BattleClass:active_count()
    local n = 0
    for _, pid in ipairs(self.rosterOrder) do
        if self.online[pid] ~= false then n = n + 1 end
    end
    return n
end

-- 周期快照 (req5): 每 kSnapshotIntervalFrames 帧序列化一次全量状态, 记录快照帧 ID,
-- 并清空重连帧缓存 (缓存从下一帧重新累积, 供 req6 重连追帧)。
function BattleClass:make_snapshot()
    if not self.core then return end
    self.lastSnapshot = { frame = self.frame, state = luacore_battle.si_battlecore_serialize(self.core) }
    self.reconnectFrames = {}
    xEngine.Log.info("[Battle] snapshot: battle=%d frame=%d bytes=%d",
        self._actor_id, self.frame, #self.lastSnapshot.state)
end

--------------------------------------------------------------------------------
-- 业务消息总入口
--------------------------------------------------------------------------------

-- 客户端直连消息分发 (由 CLuaActor::OnMessage 以 on_msg(self, msgId, body) 调用)。
-- 与 Gate 同构: msgId = (BigType<<16)|SmallType, body = [8 字节 playerId][protobuf 负载]
-- (CNetBattleGateway 已把连接身份换为 playerId 前置, 供 si_net_body_cid 还原发送方)。
-- 回包经 si_battle_gateway_send 直接下发到该玩家的直连连接 (不经 Gate)。
function BattleClass:on_msg(msgId, rawBody)
    -- 内部业务消息 (高 16 位 == USER_BEGIN=1, 如 BATTLE_FINALIZE) 与客户端直连消息
    -- (高 16 位 == AT.BATTLE=203) 共用一个 on_msg 入口, 但 msgId 空间不同:
    -- 前者须回传给基类 ActorBase:on_msg, 经 NAMES 表分发到 on_battle_finalize_msg;
    -- 后者才走下方 smallType 分发。混在一起会让 BATTLE_FINALIZE(0x00010005) 被当成
    -- smallType=5 落进 unknown 分支, 60s 回收永不执行 → 战斗房不卸载/组队房残留。
    if (msgId & 0xFFFF0000) == xEngine.MsgId.USER_BEGIN then
        return self:super("on_msg", msgId, rawBody)
    end

    local smallType = msgId & 0xFFFF
    local playerId = luacore.si_net_body_cid(rawBody)
    local payload  = luacore.si_net_body_payload(rawBody)

    local resp = ""
    if smallType == BattleMsgId.ENTER_REQ then
        resp = self:handle_enter(playerId, payload)
    elseif smallType == BattleMsgId.INPUT_REQ then
        self:handle_input(playerId, payload)
    elseif smallType == BattleMsgId.LEAVE_REQ then
        resp = self:handle_leave(playerId, payload)
    elseif smallType == BattleMsgId.HASH_NTF then
        self:handle_hash(playerId, payload)
    elseif smallType == BattleMsgId.DISCONNECT then
        self:handle_disconnect(playerId)
    elseif smallType == BattleMsgId.SPECTATOR_ENTER then
        self:handle_spectator_enter(playerId)
    else
        xEngine.Log.warn("[Battle] on_msg unknown small_type=%d", smallType)
    end

    if resp and resp ~= "" then
        luacore.si_battle_gateway_send(playerId, resp)
    end
end

function BattleClass:on_rpc_forward(wrapped)
    local senderId  = xEngine.Number.toInt(luacore.si_net_body_cid(wrapped))
    local payload   = luacore.si_net_body_payload(wrapped)
    local smallType = luacore.si_pb_small_type(payload)

    if smallType == BattleMsgId.ENTER_REQ then
        return self:handle_enter(senderId, payload)
    elseif smallType == BattleMsgId.INPUT_REQ then
        return self:handle_input(senderId, payload)
    elseif smallType == BattleMsgId.LEAVE_REQ then
        return self:handle_leave(senderId, payload)
    elseif smallType == BattleMsgId.HASH_NTF then
        return self:handle_hash(senderId, payload)
    elseif smallType == BattleMsgId.DISCONNECT then
        return self:handle_disconnect(senderId)
    end

    xEngine.Log.warn("[Battle] unknown small_type=%d", smallType)
    return ""
end

-- 进入战斗: 校验 auth_key → 初始化核心 → 回 ENTER_RSP(seed, players, frame)
function BattleClass:handle_enter(senderId, payload)
    local battleId = self._actor_id
    local authKey  = luacore.si_pb_string_field(payload, 4)

    local expected = xEngine.Redis.Get("battle:auth:" .. xEngine.Number.intKey(battleId))
    if expected == "" or authKey ~= expected then
        xEngine.Log.warn("[Battle] enter: auth failed battle=%d player=%d", battleId, senderId)
        return luacore_battle.si_pb_battle_enter_rsp(EC.BATTLE_AUTH_FAILED, battleId, 0, 0, "", self.levelId, self.difficulty)
    end

    self:ensure_core()
    local reconnect = self.frame > 0
    self.entered[senderId] = true
    self.online[senderId] = true          -- 进入/重连即恢复在线
    self.disconnectTs[senderId] = nil     -- 清除 90s 重连窗口计时
    -- 直连战斗进入/重连: 玩家确认在线, 移除下线列表成员 (此路径不经过 select_role)。
    xEngine.Redis.SRem("offline:players", xEngine.Number.intKey(senderId))
    xEngine.Redis.Del("offline:ts:" .. xEngine.Number.intKey(senderId))
    if reconnect and self.core then
        -- 断线重连 / 中途加入: 战斗已推进, 客户端无法仅凭 seed 重建当前权威状态,
        -- 故先下发快照, 再追帧重放快照帧之后缓存的所有帧 (req6), 客户端据快照反序列化 +
        -- 逐帧投递输入精确恢复到当前帧。
        self:push_reconnect_stream(senderId)
    end
    -- 同步起点: 全员 (花名册) 都进入后, 服务器广播 START_NTF, 各客户端收到后统一在帧 0
    -- 起跑 (锁步起始帧由服务器下发, 避免各端加载耗时不同导致帧号在墙钟上错位)。
    if not self.startNtfSent and #self.rosterOrder > 0 then
        local allEntered = true
        for _, pid in ipairs(self.rosterOrder) do
            if not self.entered[pid] then allEntered = false; break end
        end
        if allEntered then
            self.startNtfSent = true
            local startMsg = luacore_battle.si_pb_battle_start_ntf(battleId, self.seed)
            for _, pid in ipairs(self.rosterOrder) do
                luacore.si_battle_gateway_send(pid, startMsg)
            end
            xEngine.Log.info("[Battle] start: battle=%d seed=%d players=%d (START_NTF broadcast)",
                battleId, self.seed, #self.rosterOrder)
        end
    end

    xEngine.Log.info("[Battle] enter: battle=%d player=%d frame=%d", battleId, senderId, self.frame)
    return luacore_battle.si_pb_battle_enter_rsp(EC.NONE, battleId, self.seed, self.frame, self:players_str(), self.levelId, self.difficulty)
end

-- 客户端断线 (Gate on_rpc_disconnect 转发, req2): 标记离线、角色原地不动 (锁步补空输入),
-- 等待断线重连; 90s 内可重连, 超时按离开处理 (req8)。全员离线则调度延迟回收 (req3)。
function BattleClass:handle_disconnect(senderId)
    if not self.entered[senderId] then return "" end
    self.online[senderId] = false
    self.disconnectTs[senderId] = luacore.si_time_cur_sec()
    xEngine.Log.info("[Battle] disconnect: battle=%d player=%d (offline, %d online)",
        self._actor_id, senderId, self:active_count())

    if self:active_count() == 0 then
        self:schedule_finalize()   -- 全员离线 → 60s 回收窗口后按 all_gone 结算 (req3)
    end
    return ""
end

-- 重连恢复流 (req6): 先下发最近一次周期快照 (无则现场序列化), 再按序重放快照帧之后缓存的
-- 所有帧 (FRAME_NTF), 让客户端从快照状态精确追帧到当前帧。
function BattleClass:push_reconnect_stream(pid)
    local snapFrame, snapState
    if self.lastSnapshot then
        snapFrame = self.lastSnapshot.frame
        snapState = self.lastSnapshot.state
    else
        snapFrame = self.frame
        snapState = luacore_battle.si_battlecore_serialize(self.core)
    end
    luacore.si_battle_gateway_send(pid, luacore_battle.si_pb_battle_snapshot_ntf(snapFrame, snapState))
    for _, rec in ipairs(self.reconnectFrames) do
        luacore.si_battle_gateway_send(pid, luacore_battle.si_pb_battle_frame_ntf(rec.frameId, rec.inputsStr))
    end
    xEngine.Log.info("[Battle] reconnect stream: battle=%d player=%d snap_frame=%d cached_frames=%d",
        self._actor_id, pid, snapFrame, #self.reconnectFrames)
end

-- 逐帧输入: 按 clientFrame 锁步缓存, 全员到齐后 tick 一次 (保证服务器权威核心与
-- 客户端确定性核心同一帧内投递同一组输入, 帧号与状态哈希保持一致)。
function BattleClass:handle_input(senderId, payload)
    if not self.core then return "" end

    local clientFrame = xEngine.Number.toInt(luacore.si_pb_varint_field(payload, 4))
    local inputMsg    = luacore.si_pb_string_field(payload, 5)
    local moveDir     = xEngine.Number.toInt(luacore.si_pb_varint_field(inputMsg, 2))
    local shootDx     = xEngine.Number.toInt(luacore.si_pb_sint32_field(inputMsg, 7))
    local shootDy     = xEngine.Number.toInt(luacore.si_pb_sint32_field(inputMsg, 8))
    local skillId     = xEngine.Number.toInt(luacore.si_pb_varint_field(inputMsg, 4))
    local fire        = luacore.si_pb_bool_field(inputMsg, 5)
    local gunType     = xEngine.Number.toInt(luacore.si_pb_varint_field(inputMsg, 6))

    -- 缓存本帧输入, 待在线玩家到齐后统一投递 (离线玩家补空输入, 见 flush_frame, req2)
    self.frameInputs[clientFrame] = self.frameInputs[clientFrame] or {}
    self.frameInputs[clientFrame][senderId] = { moveDir, shootDx, shootDy, skillId, fire, gunType }
    self.frameReady[clientFrame] = (self.frameReady[clientFrame] or 0) + 1

    local active = self:active_count()
    if active > 0 and self.frameReady[clientFrame] >= active then
        self:flush_frame(clientFrame)
    end

    return ""   -- INPUT_REQ 无回包
end

-- 全员输入到齐 → 按花名册顺序投递 → tick 一次 → 检测胜负
function BattleClass:flush_frame(clientFrame)
    local inputs = self.frameInputs[clientFrame]
    if not inputs then return end

    -- 按花名册顺序 (与客户端 doTick 的 m_playerIds 顺序一致) 投递, 保证确定性。
    -- 离线玩家缺输入时补空输入 (全 0/无开火): 角色原地不动, 且空输入仍进 frameRec
    -- 广播给其它在线客户端, 保证各端确定性核心一致 (req2)。
    local frameRec = {}   -- 本帧输入录制 (战斗回放)
    for _, pid in ipairs(self.rosterOrder) do
        local inData = inputs[pid]
        if not inData and self.online[pid] == false then
            inData = { 0, 0, 0, 0, false, 0 }
        end
        if inData then
            luacore_battle.si_battlecore_submit_input(self.core, pid, inData[1], inData[2], inData[3], inData[4], inData[5], inData[6])
            frameRec[#frameRec + 1] = { pid, inData[1], inData[2], inData[3], inData[4], inData[5], inData[6] }
        end
    end
    self.replayFrames[#self.replayFrames + 1] = frameRec   -- 逐帧录制 (战斗回放)

    -- 服务器帧号必须 = 客户端锁步帧号 (clientFrame), 不能另起一个 per-flush 计数器。
    -- 两端核心的 Tick(frame) 用同一帧号做开火冷却 (frame - lastShootFrame) 与全屏炸弹
    -- 引信倒计时, 若服务器自增 self.frame 与客户端 m_nNetFrame 漂移, 同一输入会算成
    -- 不同冷却 → 哈希漂移 → 客户端把「远端输入」错位应用到错误英雄, 表现为两客户端
    -- 操作到同一角色。这里让 self.frame 直接跟随 clientFrame, 保证两端同一帧号空间。
    self.frame = clientFrame
    luacore_battle.si_battlecore_tick(self.core, self.frame)
    -- 记录本帧 tick 后的核心哈希: 客户端 HASH_NTF 上报的是「已确认帧」哈希 (该帧早已
    -- tick 过), 服务器用此处按帧存档的哈希比对, 而非当前哈希 (当前帧已领先客户端确认帧)。
    self.frameHash[self.frame] = luacore_battle.si_battlecore_hash(self.core)

    -- Actor 监控上报: 每 1 秒 (20 帧) 把本场核心状态 (BattleGame/HeroState) 缓存到 C++ Actor,
    -- 心跳线程 CActorReportMgr::Sweep 每秒把它拼进 cluster:actor:report:{type}:{id} 供监控页展示。
    if self.frame % 20 == 0 then
        self:update_report()
    end

    -- 服务器权威 push: 每帧广播 FRAME_NTF(self.frame, inputs) 给全员。
    -- 客户端据此拿到权威帧号与远端玩家真实输入 (预测校准 / 追帧 / 断线重连)。
    local insParts = {}
    for _, rec in ipairs(frameRec) do
        insParts[#insParts + 1] = table.concat({
            xEngine.Number.intKey(rec[1]), xEngine.Number.intKey(rec[2]),
            xEngine.Number.intKey(rec[3]), xEngine.Number.intKey(rec[4]),
            xEngine.Number.intKey(rec[5]),
            rec[6] and "1" or "0",
            xEngine.Number.intKey(rec[7] or 0),
        }, ",")
    end
    local insStr = table.concat(insParts, ";")
    local frameMsg = luacore_battle.si_pb_battle_frame_ntf(self.frame, insStr)
    for _, pid in ipairs(self.rosterOrder) do
        luacore.si_battle_gateway_send(pid, frameMsg)
    end
    -- 观战者追帧 (BattleActor 在本 Battle 进程向直连观战者推帧流, 不经 Gate)
    self:push_spectator_frames(self.frame, insStr)

    -- 周期快照 + 重连帧缓存 (req5/6): 快照帧本身已含本帧状态, 不重复缓存; 其余帧累积
    -- 输入串, 供断线重连时「快照 + 缓存帧」追帧恢复。
    if self.frame % kSnapshotIntervalFrames == 0 then
        self:make_snapshot()
    else
        self.reconnectFrames[#self.reconnectFrames + 1] = { frameId = self.frame, inputsStr = insStr }
    end

    -- 断线 90s 超时按离开处理 (req8); 可能进一步触发 all_gone 回收
    self:check_reconnect_timeouts()

    -- 结束条件判定: ① 胜利(所有波次清空) ② 超时失败 ③ 总死亡次数>10
    -- (全离线/全离房由 handle_disconnect / remove_player → schedule_finalize 兜底)
    self:check_end_conditions()

    self.frameInputs[clientFrame] = nil
    self.frameReady[clientFrame] = nil
end

-- 监控上报: 生成本场核心状态 JSON (BattleGame/HeroState) 并缓存到 C++ Actor。
-- BattleActor 经 ActorCreator 跨进程创建 (无 _cpp_actor), 这里用 (type,id) 定位本进程
-- 的 C++ Actor; 心跳线程 Sweep 每秒读该缓存拼进监控 Redis 键。
function BattleClass:update_report()
    if not self.core then return end
    local json = luacore_battle.si_battlecore_report_json(self.core)
    luacore.si_actor_set_report_extra(self._actor_type, self._actor_id, json)
end

-- 战斗回放: 录制校验 + 持久化
-- 用录制的逐帧输入在全新 battlecore 上重放一遍, 对比最终状态哈希与胜负,
-- 证明回放忠实可复现; 然后把输入日志落 Redis (battle:replay:{battleId}) 供后续回放。
-- winSide: 1=胜利 0=失败; duration: 战斗时长(秒)。
function BattleClass:save_replay(winSide, duration)
    local battleId = self._actor_id

    -- 1) 重放校验 (全新核心, 按录制的逐帧输入重新推进)
    local vh = luacore_battle.si_battlecore_new()
    local roster = xEngine.Redis.Get("battle:roster:" .. xEngine.Number.intKey(battleId))
    local verifyOk = luacore_battle.si_battlecore_init(vh, self.seed, roster)
    luacore_battle.si_battlecore_setup_level(vh, self.levelId, self.difficulty)   -- 回放校验核心须同配置, 否则 verify 必失配
    -- 回放校验核心须同初始主枪 (装备入初始状态), 否则 verifyHash 必失配
    for _, pid in ipairs(self.rosterOrder) do
        local p = self.players[pid]
        if p then
            luacore_battle.si_battlecore_set_gun(vh, pid, p.primaryGun or 0)
        end
    end
    local verifyHash = 0
    local verifyWin = 0
    if verifyOk then
        for frame = 1, #self.replayFrames do
            local rec = self.replayFrames[frame]
            for _, inRec in ipairs(rec) do
                luacore_battle.si_battlecore_submit_input(vh, inRec[1], inRec[2], inRec[3], inRec[4], inRec[5], inRec[6], inRec[7])
            end
            luacore_battle.si_battlecore_tick(vh, frame)
        end
        verifyHash = luacore_battle.si_battlecore_hash(vh)
        -- PvE 胜负以「所有波次清空」为准 (check_win 只判 PvP 最后幸存, 对 PvE 胜利恒为 0)
        verifyWin = luacore_battle.si_battlecore_all_waves_cleared(vh)
        luacore_battle.si_battlecore_free(vh)
    end

    local liveHash = self.core and luacore_battle.si_battlecore_hash(self.core) or 0
    local match = verifyOk and (verifyHash == liveHash) and ((verifyWin == 1) == (winSide == 1))
    xEngine.Log.info("[Battle] replay verify battle=%d frames=%d hash=%d win=%d match=%s",
        battleId, #self.replayFrames, verifyHash, verifyWin, match and "true" or "false")

    -- 2) 持久化回放日志 (格式: win|finalHash|frame0|frame1|...; 每帧 "pid,move,shoot_dx,shoot_dy,skill,fire,gunType;...")
    local parts = { xEngine.Number.intKey(win), xEngine.Number.intKey(liveHash) }
    for _, rec in ipairs(self.replayFrames) do
        local ins = {}
        for _, inRec in ipairs(rec) do
            ins[#ins + 1] = table.concat({
                xEngine.Number.intKey(inRec[1]), xEngine.Number.intKey(inRec[2]),
                xEngine.Number.intKey(inRec[3]), xEngine.Number.intKey(inRec[4]),
                xEngine.Number.intKey(inRec[5]),
                inRec[6] and "1" or "0",
                xEngine.Number.intKey(inRec[7] or 0),
            }, ",")
        end
        parts[#parts + 1] = table.concat(ins, ";")
    end
    local replayStr = table.concat(parts, "|")
    xEngine.Redis.Set("battle:replay:" .. xEngine.Number.intKey(battleId), replayStr)
    xEngine.Log.info("[Battle] replay saved battle=%d bytes=%d", battleId, #replayStr)

    -- 3) 战斗战报索引 (WebPlatform 对战报查询):
    --    battle:result:{battleId}  → 摘要 "win|finalHash|frameCount|timestamp|playerCount|roster|duration|kills"
    --    battle:index              → ZADD (score=timestamp, member=battleId) 按时间倒序列表
    local ts = luacore.si_time_cur_sec()
    local killParts = {}
    for _, pid in ipairs(self.rosterOrder) do
        killParts[#killParts + 1] = xEngine.Number.intKey(pid) .. ":" .. xEngine.Number.intKey(self.kills[pid] or 0)
    end
    local killsStr = table.concat(killParts, ";")
    local summary = table.concat({
        xEngine.Number.intKey(winSide), xEngine.Number.intKey(liveHash),
        xEngine.Number.intKey(#self.replayFrames), xEngine.Number.intKey(ts),
        xEngine.Number.intKey(#self.rosterOrder), roster,
        xEngine.Number.intKey(duration or 0), killsStr,
    }, "|")
    xEngine.Redis.Set("battle:result:" .. xEngine.Number.intKey(battleId), summary)
    luacore.si_redis_zadd("battle:index", ts, xEngine.Number.intKey(battleId))
    xEngine.Log.info("[Battle] result saved battle=%d win=%d frames=%d duration=%d ts=%d",
        battleId, winSide, #self.replayFrames, duration or 0, ts)
end

--------------------------------------------------------------------------------
-- 结束条件 / 结算 / 生命周期
--------------------------------------------------------------------------------

-- 每帧 tick 后判定结束条件 (Lua 权威, 核心只提供查询):
--   ① 胜利: 所有波次怪物都被击杀 (allWavesCleared)
--   ② 失败: 战斗时间结束 (frame >= 时间上限*帧率)
--   (全离线由 handle_leave 触发 1 分钟重连窗口后回收)
function BattleClass:check_end_conditions()
    if self.finished or not self.core then return end
    if luacore_battle.si_battlecore_all_waves_cleared(self.core) == 1 then
        self:finish_battle("win", true)
    elseif self.frame >= kBattleTimeLimitSec * kFrameRate then
        self:finish_battle("timeout", false)
    elseif luacore_battle.si_battlecore_total_deaths(self.core) > kMaxTotalDeaths then
        self:finish_battle("deaths", false)   -- 全员累计死亡次数超限 (req3)
    end
end

-- 统一结束入口: 上报战报 → 结算 → 广播 END_NTF → 释放核心 → 延迟卸载
function BattleClass:finish_battle(reason, win)
    if self.finished then return end
    self.finished  = true
    self.endReason = reason
    self.duration  = (self.startTs > 0) and (luacore.si_time_cur_sec() - self.startTs) or 0
    local winSide  = win and 1 or 0

    -- 击杀数 (结算/战报共用; 须在释放核心前读)
    self.kills = self:compute_kills()

    -- 上报战报 (回放校验 + battle:replay + battle:result + battle:index)
    self:save_replay(winSide, self.duration)

    -- 结算: 胜发击杀排名奖励 / 败仅扣入场费, 回传 PlayerActor
    self:settle(win)

    -- 广播 END_NTF (stats = 可读战报串)
    local stats = self:build_stats(win, self.duration)
    local endMsg = luacore_battle.si_pb_battle_end_ntf(self._actor_id, winSide, stats)
    for _, pid in ipairs(self.rosterOrder) do
        luacore.si_battle_gateway_send(pid, endMsg)
    end
    self:push_spectate_end()

    -- 战斗结束: 断线未归 (online==false) 的玩家已无法再重连, 逐个通知 Room 移出组队房
    -- (LEAVE_REQ → Room do_leave 删空房)。否则 Room 侧 count_members 恒 >0, 空房不删,
    -- 组队房残留幽灵成员 (全离线 finalize 前 remove_player 不会跑, 见 check_reconnect_timeouts)。
    for _, pid in ipairs(self.rosterOrder) do
        if self.online[pid] == false then
            luacore.si_forward_rpc(AT.ROOM, 0,
                luacore.si_net_body_wrap(pid, encode_small_type(ROOM_SMALL.LEAVE_REQ)))
        end
    end

    -- 战斗结束: 通知 Room 重置状态, 所有玩家回到组队房间 (房间保留, state 回 0)。
    -- battle_id 塞进 wrap 的 cid 位, Room 经 battle:room:{battleId} 反查房间。
    luacore.si_forward_rpc(AT.ROOM, 0,
        luacore.si_net_body_wrap(self._actor_id, encode_small_type(ROOM_SMALL.BATTLE_END)))

    -- 立即清 battle:byplayer (不等 60s finalize): 结算窗口内玩家断线时, Gate
    -- on_rpc_disconnect 据此判断「是否在战斗中」, 残留会把断线误路由到已结束的 Battle
    -- (Room 收不到 LEAVE_REQ → 组队房残留)。CAS 保护: 只清仍指向本场的映射。
    local bid = xEngine.Number.intKey(self._actor_id)
    for _, pid in ipairs(self.rosterOrder) do
        local pidStr = xEngine.Number.intKey(pid)
        if xEngine.Redis.Get("battle:byplayer:" .. pidStr) == bid then
            xEngine.Redis.Del("battle:byplayer:" .. pidStr)
        end
    end

    xEngine.Log.info("[Battle] finish: battle=%d reason=%s win=%s duration=%d totalKills=%d",
        self._actor_id, reason, tostring(win), self.duration, self:kill_total())

    -- 释放战斗核心 (大对象; 之后 handle_input/handle_hash 因 self.core==nil 直接短路)
    if self.core then
        luacore_battle.si_battlecore_free(self.core)
        self.core = nil
    end

    -- 1 分钟延迟之后删除卸载 (用户第 9 条)
    self:schedule_finalize()
end

function BattleClass:kill_total()
    local n = 0
    for pid in pairs(self.kills) do n = n + (self.kills[pid] or 0) end
    return n
end

function BattleClass:spectator_count()
    local n = 0
    for _ in pairs(self.spectators) do n = n + 1 end
    return n
end

function BattleClass:compute_kills()
    local kills = {}
    for _, pid in ipairs(self.rosterOrder) do
        kills[pid] = luacore_battle.si_battlecore_get_hero_kills(self.core, pid)
    end
    return kills
end

-- 结算: 按击杀数量降序排名 (并列按花名册序稳定), 胜方发放排名奖励 + 基础参与经验;
-- 败方无奖励。金币入场费对所有参战者扣减 (钳位 ≥0)。
function BattleClass:settle(win)
    local ranked = {}
    for _, pid in ipairs(self.rosterOrder) do ranked[#ranked + 1] = pid end
    table.sort(ranked, function(a, b)
        local ka, kb = (self.kills[a] or 0), (self.kills[b] or 0)
        if ka ~= kb then return ka > kb end
        return a < b
    end)
    self.ranked = ranked

    for rank, pid in ipairs(ranked) do
        local tier = RewardTier[rank]
        local dExp, dGold = 0, -kGoldEntryFee
        if win then
            dExp  = kParticipateExp + (tier and tier.exp or 0)
            dGold = dGold + (tier and tier.gold or 0)
        end
        self.settleResult[pid] = { rank = rank, exp = dExp, gold = dGold, kills = self.kills[pid] or 0 }
        self:deliver_settle(pid, dExp, dGold, rank)
    end
end

-- 回传结算到 PlayerActor (进程内 post_message 优先; 跨进程/离线走 Redis 兜底)
function BattleClass:deliver_settle(pid, dExp, dGold, rank)
    local body = string.format("%d|%d|%d|%d", dExp, dGold, kGoldEntryFee, rank)
    local pCpp = luacore.si_actor_mgr_find(AT.PLAYER, pid)
    if pCpp then
        luacore.si_actor_post_message(pCpp, xEngine.MsgId.PLAYER_SETTLE, body)
        xEngine.Log.info("[Battle] settle: player=%d rank=%d dExp=%d dGold=%d (actor)",
            pid, rank, dExp, dGold)
    else
        xEngine.Role.updateAttrs(pid, { exp = dExp, gold = dGold })
        xEngine.Log.info("[Battle] settle: player=%d rank=%d dExp=%d dGold=%d (redis fallback)",
            pid, rank, dExp, dGold)
    end
end

-- 战报 stats 串 (写入 PBBattleEndNtf.stats, 客户端展示/WebPlatform 查询)
--   "win|duration|pid:kills:rank:exp:gold;pid:kills:rank:exp:gold;..."
function BattleClass:build_stats(win, duration)
    local parts = { win and "1" or "0", xEngine.Number.intKey(duration) }
    for _, pid in ipairs(self.ranked or {}) do
        local r = self.settleResult[pid]
        parts[#parts + 1] = table.concat({
            xEngine.Number.intKey(pid),
            xEngine.Number.intKey(r and r.kills or 0),
            xEngine.Number.intKey(r and r.rank or 0),
            xEngine.Number.intKey(r and r.exp or 0),
            xEngine.Number.intKey(r and r.gold or 0),
        }, ":")
    end
    return table.concat(parts, ";")
end

-- 延迟卸载调度 (1 分钟; 幂等)
function BattleClass:schedule_finalize()
    if self.finalizeScheduled then return end
    self.finalizeScheduled = true
    luacore.si_schedule_after(AT.BATTLE, self._actor_id, kBattleCleanupDelayMs,
        xEngine.MsgId.BATTLE_FINALIZE, "")
    xEngine.Log.info("[Battle] finalize scheduled battle=%d delay=%dms", self._actor_id, kBattleCleanupDelayMs)
end

-- 延迟回收 (MSG.BATTLE_FINALIZE): 重连窗口内有人回来则放弃回收; 否则补报战报后卸载
function BattleClass:on_battle_finalize_msg(raw_body)
    if not self.finished and self:active_count() > 0 then
        self.finalizeScheduled = false   -- 有人回来 (在线), 放弃本次回收
        xEngine.Log.info("[Battle] finalize aborted: players rejoined battle=%d", self._actor_id)
        return
    end
    if not self.finished then
        -- 游戏未结束但所有玩家都离开 → 按失败上报战报 (无奖励)
        self:finish_battle("all_left", false)
    end
    -- 卸载 BattleActor (本地延迟 delete) + 全局删除 SpectatorActor (惰性创建, 可能不驻本进程,
    -- 经 cluster:actor:{type}:{id} 路由到其实际进程卸载) + 清理观战登记
    luacore.si_actor_delete_local(AT.BATTLE, self._actor_id)
    xEngine.ActorCreator.delete(AT.SPECTATOR, self._actor_id)
    local bid = xEngine.Number.intKey(self._actor_id)
    -- 清理观战名单 + 观战反查映射 (spectator.lua handle_enter 写入 battle:spectator:byplayer:{pid}),
    -- 防止残留映射指向已死战斗房 (观战者断线未走 handle_leave 时, 名单在此兜底清空)。
    local spectatorList = xEngine.Redis.Get("battle:spectator:" .. bid) or ""
    xEngine.Redis.Del("battle:spectator:" .. bid)
    for pidStr in (spectatorList .. ","):gmatch("([^,]*),") do
        if pidStr ~= "" then
            if xEngine.Redis.Get("battle:spectator:byplayer:" .. pidStr) == bid then
                xEngine.Redis.Del("battle:spectator:byplayer:" .. pidStr)
            end
        end
    end

    -- 清理观战反查映射 (room.lua handle_start 写入), 防止残留映射指向已死战斗房:
    --   battle:byroom:{roomId} / battle:byplayer:{pid} / battle:room:{battleId}
    -- 比较后删除 (CAS 语义): 玩家可能在结算窗口内进入下一局, byplayer 已被改写,
    -- 此时不可误删下一局的映射。
    local roomId = xEngine.Redis.Get("battle:room:" .. bid)
    if roomId and roomId ~= "" then
        if xEngine.Redis.Get("battle:byroom:" .. roomId) == bid then
            xEngine.Redis.Del("battle:byroom:" .. roomId)
        end
    end
    xEngine.Redis.Del("battle:room:" .. bid)
    local roster = xEngine.Redis.Get("battle:roster:" .. bid) or ""
    for pidStr in (roster .. ","):gmatch("([^,]*),") do
        if pidStr ~= "" then
            if xEngine.Redis.Get("battle:byplayer:" .. pidStr) == bid then
                xEngine.Redis.Del("battle:byplayer:" .. pidStr)
            end
        end
    end
    xEngine.Redis.Del("battle:roster:" .. bid)
    xEngine.Redis.Del("battle:host:" .. bid)

    xEngine.Log.info("[Battle] finalize done: battle=%d reason=%s", self._actor_id, tostring(self.endReason))
end

--------------------------------------------------------------------------------
-- 观战 (权威名单在 Redis, 由本 Battle 进程 SpectatorActor 登记; 本 Battle 进程向直连观战者推流)
--------------------------------------------------------------------------------
function BattleClass:refresh_spectators()
    local raw = xEngine.Redis.Get("battle:spectator:" .. xEngine.Number.intKey(self._actor_id))
    local newList = {}
    for v in (raw or ""):gmatch("[^,]+") do
        if v ~= "" then newList[tonumber(v)] = true end
    end
    self.spectators = newList
    self.spectatorCacheFrame = self.frame
end

-- 观战者直连登录 (CNetBattleGateway 握手成功后投递 SPECTATOR_ENTER): 登记 + 下推快照对齐。
-- 观战者与参战者共用同一套 203 帧流, 快照/帧/结束均经 si_battle_gateway_send 直发 (不经 Gate)。
function BattleClass:handle_spectator_enter(pid)
    self.spectators[pid] = true
    self:push_spectator_snapshot(pid)
end

function BattleClass:push_spectator_snapshot(pid)
    if not self.core then return end
    local state = luacore_battle.si_battlecore_serialize(self.core)
    local snap  = luacore_battle.si_pb_battle_snapshot_ntf(self.frame, state)
    luacore.si_battle_gateway_send(pid, snap)
    xEngine.Log.info("[Battle] spectator snapshot: battle=%d spectator=%d frame=%d", self._actor_id, pid, self.frame)
end

function BattleClass:push_spectator_frames(frameId, inputsStr)
    -- 每 10 帧刷新一次观战名单 (避免每帧 Redis 读; 新观战者最迟 0.5s 内被纳入)
    if self.frame % 10 == 0 then self:refresh_spectators() end
    if next(self.spectators) == nil then return end
    local frameMsg = luacore_battle.si_pb_battle_frame_ntf(frameId, inputsStr)
    for pid in pairs(self.spectators) do
        luacore.si_battle_gateway_send(pid, frameMsg)
    end
end

function BattleClass:push_spectate_end()
    local endMsg = luacore_battle.si_pb_battle_end_ntf(self._actor_id, 0, "")
    for pid in pairs(self.spectators) do
        luacore.si_battle_gateway_send(pid, endMsg)
    end
end

-- 中途退出
function BattleClass:handle_leave(senderId, payload)
    xEngine.Log.info("[Battle] leave: battle=%d player=%d", self._actor_id, senderId)
    self:remove_player(senderId)
    return luacore_battle.si_pb_battle_leave_rsp(EC.NONE)
end

-- 统一移除玩家 (主动离开 / 断线 90s 超时 / 全员回收共用): 清出战状态 + 有序花名册,
-- 补冲刷因移除而解冻的缓冲帧, 通知 Room 离开组队房 (req9), 无活跃玩家则调度回收 (req3)。
function BattleClass:remove_player(pid)
    self.entered[pid] = nil
    self.players[pid] = nil
    self.online[pid] = nil
    self.disconnectTs[pid] = nil

    -- 关键: 同步从有序花名册移除。flush_frame 的触发条件是 frameReady[frame] >= active_count,
    -- 退出玩家不再上报 INPUT_REQ, 若仍留在 rosterOrder 会让帧循环永久等待 → tick 停滞 →
    -- 监控上报 (update_report 在 flush_frame 内) 也随之停止。移除后剩余玩家继续锁步推进。
    for i = #self.rosterOrder, 1, -1 do
        if self.rosterOrder[i] == pid then
            table.remove(self.rosterOrder, i)
            break
        end
    end

    -- 退出玩家缺输入卡住的缓冲帧, 现在剩余在线玩家已到齐 → 按帧号顺序补冲刷, 恢复 tick 与上报。
    -- (按 self.frame+1 顺序 flush, 保证 si_battlecore_tick 帧号连续, 不破坏确定性。)
    local active = self:active_count()
    if active > 0 then
        while self.core and self.frameInputs[self.frame + 1]
              and (self.frameReady[self.frame + 1] or 0) >= active do
            self:flush_frame(self.frame + 1)
        end
    end

    -- 中途退出: 通知 Room 让该玩家直接离开组队房间 (复用 LEAVE_REQ, roomId 缺省由
    -- Room 依 playerRoom[pid] 反查, 与断线收尾链路同款)。
    luacore.si_forward_rpc(AT.ROOM, 0,
        luacore.si_net_body_wrap(pid, encode_small_type(ROOM_SMALL.LEAVE_REQ)))

    -- 所有玩家都离开/离线且战斗未结束: 给 1 分钟重连窗口后回收; 已结束由 finish_battle 调度, 无需重复。
    if self:active_count() == 0 and not self.finished then
        self:schedule_finalize()
    end
end

-- 断线 90s 超时 (req8): 超时玩家不能再断线重连, 直接按离开处理 (remove_player → 离开组队房)。
-- 每帧在 flush_frame 内调用 (游戏存活时由在线玩家驱动帧推进; 全离线已走 finalize 回收)。
function BattleClass:check_reconnect_timeouts()
    if not next(self.disconnectTs) then return end
    local now = luacore.si_time_cur_sec()
    local expired = {}
    for pid, ts in pairs(self.disconnectTs) do
        if now - ts >= kReconnectWindowSec then expired[#expired + 1] = pid end
    end
    for _, pid in ipairs(expired) do
        xEngine.Log.info("[Battle] reconnect timeout: battle=%d player=%d (offline %ds)",
            self._actor_id, pid, now - self.disconnectTs[pid])
        self:remove_player(pid)
    end
end

-- 一致性哈希上报 (客户端→服务器, 无回包)
--
-- si_battlecore_hash 现在返回 int64 (uint64 位重解释), 与 si_pb_varint_field
-- 读回客户端哈希的 signed 表示一致, 二者都是精确的 64 位 Lua 整数, 可直接 %d
-- 打印与 == 比较。客户端上报的是「已确认帧」哈希 (该帧服务器早已 tick 过并归档进
-- self.frameHash), 故这里按 frameId 取回该帧存档哈希比对, 而非当前哈希 —— 服务器
-- 当前帧已领先客户端确认帧, 拿当前哈希比对会因时序错位而永远失配。
function BattleClass:handle_hash(senderId, payload)
    local frameId    = xEngine.Number.toInt(luacore.si_pb_varint_field(payload, 4))
    local clientHash = luacore.si_pb_varint_field(payload, 5)
    local serverHash = self.frameHash[frameId]
    if serverHash ~= nil then
        if serverHash ~= clientHash then
            xEngine.Log.warn("[Battle] hash mismatch: battle=%d frame=%d client=%d server=%d",
                self._actor_id, frameId, clientHash, serverHash)
            -- 预测和解: 检测到客户端状态漂移, 下发权威全量快照, 客户端据此校准后追帧
            if self.core then
                local state = luacore_battle.si_battlecore_serialize(self.core)
                local snap = luacore_battle.si_pb_battle_snapshot_ntf(self.frame, state)
                luacore.si_battle_gateway_send(senderId, snap)
                xEngine.Log.info("[Battle] push snapshot reconcile: battle=%d player=%d frame=%d",
                    self._actor_id, senderId, self.frame)
            end
        else
            xEngine.Log.info("[Battle] hash ok: battle=%d frame=%d hash=%d",
                self._actor_id, frameId, clientHash)
        end
    end
    return ""
end

-- GM 命令: 报告战斗房当前状态 (WebPlatform GM 控制台经 `actor 203 <id> status` 触发)
function BattleClass:on_gm_cmd(cmdline)
    if cmdline == "status" or cmdline == "" then
        return string.format("battle=%d frame=%d players=%d finished=%s reason=%s duration=%d core=%s replay_frames=%d spectators=%d",
            self._actor_id, self.frame, #self.rosterOrder,
            tostring(self.finished), tostring(self.endReason or "-"), self.duration or 0,
            self.core and "yes" or "no", #self.replayFrames, self:spectator_count())
    end
    return string.format("battle=%d: unknown gm cmd '%s' (try 'status')",
        self._actor_id, tostring(cmdline))
end

xEngine.ActorMgr.registerPrototype(AT.BATTLE, BattleClass)
xEngine.ActorMgr.install(AT.BATTLE, 0)
