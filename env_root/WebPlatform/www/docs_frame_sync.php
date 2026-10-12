<?php
// =============================================================================
// 帧同步架构文档 — 服务器 ↔ 客户端帧同步全流程 + 全部数据字段
//
// 技术文档模块 / 战斗。静态文档页，无动态数据。
// 覆盖: 同步模型、身份链、开战流程、帧循环、一致性校验、协议字段、核心状态、Redis 键。
// =============================================================================
require_once __DIR__ . '/_layout.php';

layout_header('帧同步架构', 'docs_frame_sync');
?>
<div class="summary">
    <div class="stat-grid">
        <div class="stat"><div class="k">同步模型</div><div class="v">共享核心 Lockstep<small>确定性帧同步</small></div></div>
        <div class="stat"><div class="k">逻辑帧率</div><div class="v">20Hz<small>kFixedDeltaMs=50ms/帧</small></div></div>
        <div class="stat"><div class="k">数值精度</div><div class="v">Q16.16<small>定点数，逐位一致</small></div></div>
        <div class="stat"><div class="k">权威归属</div><div class="v">服务端权威<small>客户端预测 + 回滚</small></div></div>
    </div>
    <p>
        战斗采用<b>确定性帧同步 + 服务端权威</b>模型：客户端与服务端编译<b>同一份</b>战斗核心
        <code class="doc-path">shared/battlecore</code>（CBattleCore），客户端通过 <code>CPredictiveSim</code>
        做 run-ahead 预测，服务端 BattleActor 以 20Hz 锁步权威模拟后下发 FRAME_NTF，客户端据此回滚/校准，
        保证两端状态逐位一致。数值用 Q16.16 定点，跨平台确定性由 <code>BattleRng</code>（同种子同序列）保证。
    </p>
</div>

<!-- ================= 关键不变量 ================= -->
<div class="card">
    <div class="card-header">关键不变量 <span class="dim">决定「两端同一帧算出同一状态」的四条铁律</span></div>
    <div class="card-body">
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>不变量</th><th>说明</th><th>实现位置</th></tr></thead>
            <tbody>
                <tr><td>同源码</td><td>C/S 编译同一份 shared/battlecore，逻辑逐位一致</td><td>shared/battlecore/</td></tr>
                <tr><td>同帧号</td><td>服务器帧号 <code>self.frame</code> 直接跟随客户端 <code>clientFrame</code>，两端共用同一帧号空间</td><td>script/actors/battle/battle.lua flush_frame</td></tr>
                <tr><td>同花名册序</td><td>花名册顺序即 spawn_index 序，两端英雄→player_id 映射一致</td><td>battle:roster / UINetBattleLayer::onEnterRsp</td></tr>
                <tr><td>同输入归因</td><td>服务器以 <code>senderId</code>（连接身份）归因输入，不信任客户端自报 player_id</td><td>battle.lua handle_input</td></tr>
            </tbody>
        </table></div>
        <div class="dim">
            帧号对齐是锁步一致性的根基：<code>Tick(frame)</code> 里开火冷却 <code>frame - lastShootFrame</code>、
            全屏炸弹引信倒计时都依赖帧号，若服务器另起 per-flush 计数器与客户端 <code>m_nNetFrame</code> 漂移，
            同一输入会算成不同冷却 → 哈希漂移 → 回滚错位，表现为「两客户端操作到同一角色」。
        </div>
    </div>
</div>

<!-- ================= 身份链 ================= -->
<div class="card">
    <div class="card-header">身份链 <span class="dim">玩家 ID 如何从连接一路贯通到英雄</span></div>
    <div class="card-body">
        <pre class="doc-pre">连接 cid
  └─ gate.lua handle_select_role   →  connPlayer[cid] = roleId
roleId = accountId * 1000 + count + 1        (login.lua rpc_create_role)
playerId = roleId                            (login.lua rpc_select_role 校验角色归属后)
客户端 m_nNetPlayerId = playerId             (SelectRoleRsp → PlayerInfo)
  └─ gate.lua forward_business      →  si_net_body_wrap(playerId, payload)
服务器 senderId = si_net_body_cid(wrapped)   = playerId  ← 输入归因用它
room.members 键 = senderId                    (handle_join/handle_create)
battle:roster = 逗号分隔的 members 键         (room.lua handle_start)
core Init: h.player_id = playerIds[i]         (i 为花名册序, spawn_index=i)
core Step:  按 p.player_id == h.player_id 匹配英雄</pre>
        <div class="dim">
            <code>si_net_body_wrap</code> / <code>si_net_body_cid</code> / <code>si_net_body_payload</code> 是对称的
            8 字节小端 <code>[8-byte id][payload]</code> 帧格式。花名册由 <code>pairs(room.members)</code> 写出，
            顺序非确定但<b>一旦写入即稳定</b>，两端据此得到一致的 player_id 数组与英雄映射。
        </div>
    </div>
</div>

<!-- ================= 开战流程 ================= -->
<div class="card">
    <div class="card-header">组队开战流程 <span class="dim">建房 → 开局 → 进入战斗</span></div>
    <div class="card-body">
        <pre class="doc-pre">1. 房主 room.lua handle_create      → members[ownerId] 登记
2. 队友 room.lua handle_join        → members[joinerId] 登记
3. 双方 handle_ready                → ready = true
4. 房主 handle_start:
     写 Redis  battle:roster / battle:auth / battle:level / battle:equip
     balanceCreate(AT.BATTLE, battleId)    跨进程建 Battle Actor
     balanceCreate(AT.SPECTATOR, battleId) 跨进程建观战 Actor
     房主收 START_RSP, 队友收 START_NTF
5. 客户端发 EBattle.EnterReq(battle_id, auth_key, player_id)
     → gate.lua forward_business  wrap(playerId) → si_forward_rpc(AT.BATTLE, battleId)
6. battle.lua handle_enter:
     校验 auth → ensure_core:
       si_battlecore_init(roster)
       si_battlecore_setup_level(level, difficulty)
       逐 pid si_battlecore_set_gun(装备)
     回 EnterRsp(seed, players, level, difficulty)
7. 客户端 UINetBattleLayer::onEnterRsp:
     按 pid == m_playerId 区分自己/远端, enterNetBattle 初始化 m_pred
     (花名册按 spawnIndex 升序 = 服务器 rosterOrder)</pre>
    </div>
</div>

<!-- ================= 帧循环 ================= -->
<div class="card">
    <div class="card-header">帧循环 <span class="dim">20Hz / 50ms 固定步长，主数据通路</span></div>
    <div class="card-body">
        <pre class="doc-pre">客户端 StageScene::update                      服务端 BattleActor
  │ m_fNetAccum += dt                             │
  │ while(>=50ms):                                │
  │   AdvanceBattleClock() → m_nNetFrame          │
  │   sampleNetInput() (player_id=m_nNetPlayerId) │
  │   sendInputReq(client_frame, input)           │
  ├───────────── EInputReq ──────────────────────▶│ handle_input(senderId, payload)
  │                                               │   clientFrame = field 4
  │                                               │   frameInputs[clientFrame][senderId] = input
  │                                               │   frameReady[clientFrame]++
  │                                               │   （全员到齐 ≥ #rosterOrder 才 flush）
  │                                               │ flush_frame(clientFrame):
  │                                               │   按 rosterOrder 序 submit_input
  │                                               │   self.frame = clientFrame  ★
  │                                               │   si_battlecore_tick(self.frame)
  │                                               │   frameHash[self.frame] = hash
  │◀───────────── EFrameNtf ──────────────────────┤   广播 FRAME_NTF(self.frame, 全员输入)
  │ onFrameNtf → onNetFrame 缓存                  │
  │ drainServerFrames (LastConfirmedFrame()+1)    │
  │ OnServerFrame: diff / 回滚 / 重放             │
  │ predictLocalFrame: 本地真实输入 + 远端最近输入 │
  │ renderCoreState() 读 m_pred.Core() 驱动渲染    │</pre>
        <div class="dim">
            ★ 服务器帧号直接取 <code>clientFrame</code>（客户端 <code>m_nNetFrame</code>），不另起自增计数器，
            保证 <code>Tick(frame)</code> 的开火冷却 / 炸弹引信在两端用<b>同一帧号</b>计算。客户端 run-ahead
            预测不用等服务器帧，本地即时响应；服务器权威帧到达后回滚到已确认帧、重放权威输入，再续预测。
        </div>
    </div>
</div>

<!-- ================= 一致性校验 ================= -->
<div class="card">
    <div class="card-header">一致性校验与纠偏</div>
    <div class="card-body">
        <pre class="doc-pre">客户端每 50 帧  → EHashNtf(frame_id, state_hash)  → 服务器 handle_hash
  比对 frameHash[frameId]
    ├─ 一致: 继续
    └─ 失配: 服务器发 ESnapshotNtf(frame, state)
             客户端 onSnapshot → ResyncFromSnapshot 全量纠偏</pre>
        <div class="dim">
            哈希覆盖 CBattleCore 序列化状态（magic/seed/rng/frame/nextId + 游戏状态），
            失配即触发全量快照兜底，保证长时间运行不累积漂移。
        </div>
    </div>
</div>

<!-- ================= 结束流程 ================= -->
<div class="card">
    <div class="card-header">结束与战报落库</div>
    <div class="card-body">
        <pre class="doc-pre">flush_frame 末 check_end_conditions
  ├─ 全波清空 = 胜利
  └─ frame ≥ 180×20 = 超时失败
finish_battle:
  save_replay  → battle:replay  (逐帧输入回放, 供观战/回放)
  settle       → battle:result  (战报摘要)  + battle:index (ZADD 时间倒序)
  广播 EEndNtf → 结算 → 延迟卸载 (DeleteActor)</pre>
    </div>
</div>

<!-- ================= 协议字段 ================= -->
<div class="card">
    <div class="card-header">协议字段文档 <span class="dim">source/proto-def/msg/battle.proto</span></div>
    <div class="card-body">
        <h3 class="doc-h3">PBBattlePlayerInput（单帧输入）</h3>
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>字段</th><th>号</th><th>类型</th><th>含义</th></tr></thead>
            <tbody>
                <tr><td>player_id</td><td>1</td><td>int64</td><td>玩家 ID（客户端自报；服务器改用 senderId 归因）</td></tr>
                <tr><td>move_dir</td><td>2</td><td>uint32</td><td>移动方向 0-8（0=停,1=上,2=右上,3=右,4=右下,5=下,6=左下,7=左,8=左上）</td></tr>
                <tr><td>skill_id</td><td>4</td><td>uint32</td><td>技能 ID（0=无；kSkillFullScreenBomb=全屏炸弹）</td></tr>
                <tr><td>fire</td><td>5</td><td>bool</td><td>是否开火</td></tr>
                <tr><td>gun_type</td><td>6</td><td>uint32</td><td>枪械类别 kGun*（手枪0/机枪1/散弹2/狙击3/加特林16/火箭筒17）</td></tr>
                <tr><td>shoot_dx</td><td>7</td><td>sint32</td><td>射击方向 X（Q16.16 定点，数学坐标 Y 向上）</td></tr>
                <tr><td>shoot_dy</td><td>8</td><td>sint32</td><td>射击方向 Y（Q16.16 定点）</td></tr>
            </tbody>
        </table></div>

        <h3 class="doc-h3">PBBattlePlayer（花名册玩家）</h3>
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>字段</th><th>号</th><th>类型</th><th>含义</th></tr></thead>
            <tbody>
                <tr><td>player_id</td><td>1</td><td>int64</td><td>玩家 ID</td></tr>
                <tr><td>name</td><td>2</td><td>string</td><td>昵称</td></tr>
                <tr><td>spawn_index</td><td>3</td><td>uint32</td><td>出生点索引（= 花名册顺序）</td></tr>
                <tr><td>primary_gun</td><td>4</td><td>uint32</td><td>初始主枪（kGun*）</td></tr>
                <tr><td>equip</td><td>5</td><td>string</td><td>6 槽枪械（逗号分隔，NONE=22）</td></tr>
                <tr><td>level</td><td>6</td><td>uint32</td><td>角色等级</td></tr>
                <tr><td>exp</td><td>7</td><td>uint32</td><td>经验</td></tr>
                <tr><td>gold</td><td>8</td><td>uint32</td><td>金币</td></tr>
                <tr><td>attack</td><td>9</td><td>uint32</td><td>攻击</td></tr>
                <tr><td>defense</td><td>10</td><td>uint32</td><td>防御</td></tr>
                <tr><td>hp</td><td>11</td><td>uint32</td><td>血量（战斗内实时值由核心 HeroState 覆盖）</td></tr>
            </tbody>
        </table></div>

        <h3 class="doc-h3">各消息关键字段</h3>
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>消息</th><th>字段</th></tr></thead>
            <tbody>
                <tr><td>PBBattleEnterReq</td><td>battle_id=3, auth_key=4, player_id=5</td></tr>
                <tr><td>PBBattleEnterRsp</td><td>error_code=3, battle_id=4, seed=5, players=6(repeated), max_frame=7, level_id=8, difficulty=9</td></tr>
                <tr><td>PBBattleInputReq</td><td>battle_id=3, client_frame=4, input=5(子消息)</td></tr>
                <tr><td>PBBattleFrameNtf</td><td>frame_id=3, inputs=4(repeated PBBattlePlayerInput)</td></tr>
                <tr><td>PBBattleSnapshotNtf</td><td>frame_id=3, state=4(bytes)</td></tr>
                <tr><td>PBBattleEndNtf</td><td>battle_id=3, win_side=4, stats=5</td></tr>
                <tr><td>PBBattleLeaveReq</td><td>battle_id=3, player_id=4</td></tr>
                <tr><td>PBBattleHashNtf</td><td>battle_id=3, frame_id=4, state_hash=5</td></tr>
            </tbody>
        </table></div>
    </div>
</div>

<!-- ================= 核心状态 ================= -->
<div class="card">
    <div class="card-header">核心状态字段 <span class="dim">shared/battlecore</span></div>
    <div class="card-body">
        <h3 class="doc-h3">CBattleCore 序列化快照（SerializeState，魔数 0x42435431 = "BCT1"）</h3>
        <pre class="doc-pre">magic | seed | rng 状态 | frame | nextId | 游戏状态</pre>
        <h3 class="doc-h3">BattleGame 游戏状态（随快照/哈希）</h3>
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>成员</th><th>说明</th></tr></thead>
            <tbody>
                <tr><td>m_heroes</td><td>HeroState 数组（见下）</td></tr>
                <tr><td>m_sp01s</td><td>小怪 / boss</td></tr>
                <tr><td>m_bullets</td><td>子弹</td></tr>
                <tr><td>m_turrets</td><td>炮台</td></tr>
                <tr><td>m_props</td><td>道具</td></tr>
                <tr><td>m_wave</td><td>波次状态</td></tr>
                <tr><td>m_game</td><td>对局状态（进行/胜负）</td></tr>
                <tr><td>m_nextId</td><td>实体 id 分配器</td></tr>
            </tbody>
        </table></div>
        <h3 class="doc-h3">HeroState（英雄确定性状态）</h3>
        <pre class="doc-pre">id, player_id, x/y(定点), hp/maxHp, walkSpeed, speedMul, facing,
action, animDir, alive, gunType, reportedGun, lastShootFrame, moveDir,
aimRadians, bombFuse, killCount</pre>
        <div class="dim">
            关键常量：<code>kLogicFps=20</code>、<code>kFixedDeltaMs=50</code>、<code>kHpScale=100</code>（血量 centi）、
            英雄默认移速 <code>6.875 px/帧</code>、出生点 <code>x=160×(i+1), y=240</code>。
        </div>
        <h3 class="doc-h3">场景对象（小怪 / 炮台，随监控上报）</h3>
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>结构</th><th>字段</th><th>说明</th></tr></thead>
            <tbody>
                <tr><td rowspan="7">Sp01State<br>（怪物）</td><td>id / classId</td><td>实体 ID / 怪物类别（kClassSp01*）</td></tr>
                <tr><td>x / y</td><td>坐标（Q16.16 定点，监控上报按 FxToIntRound 转整数像素）</td></tr>
                <tr><td>hp / maxHp</td><td>生命 / 最大生命（坦度）</td></tr>
                <tr><td>hurtToHero</td><td>攻击（对英雄单次伤害，定点；监控上报字段 atk）</td></tr>
                <tr><td>action / animDir</td><td>动作状态 / 8 方向</td></tr>
                <tr><td>walkSpeed</td><td>走速 px/帧</td></tr>
                <tr><td>remove</td><td>待移除标记（监控上报跳过已死亡实体）</td></tr>
                <tr><td rowspan="4">TurretState<br>（炮台）</td><td>x / y</td><td>坐标（定点）</td></tr>
                <tr><td>hp / maxHp</td><td>生命 / 最大生命</td></tr>
                <tr><td>hurtToHero</td><td>炮弹对英雄伤害（监控上报字段 atk）</td></tr>
                <tr><td>active</td><td>是否已激活（监控上报字段 active）</td></tr>
            </tbody>
        </table></div>
        <div class="dim">
            说明：战斗核心<b>没有独立的「防御」属性</b>，怪物/炮台的坦度由 <code>hp/maxHp</code> 表达，「攻击」即
            <code>hurtToHero</code>（对英雄单次伤害）。BattleActor 监控上报（<code>cluster:actor:report:203:{id}</code>）
            的 <code>battle</code> 子对象新增 <code>monsters[]</code>（id/cls/x/y/hp/max_hp/atk/action）与
            <code>turrets[]</code>（id/x/y/hp/max_hp/atk/active），由 <code>si_battlecore_report_json</code>
            生成、心跳 Sweep 每 1 秒写入，供 actor_info.php 展示。
        </div>
    </div>
</div>

<!-- ================= Redis 键 ================= -->
<div class="card">
    <div class="card-header">Redis 键文档 <span class="dim">战斗相关键</span></div>
    <div class="card-body">
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>键</th><th>格式</th><th>说明</th></tr></thead>
            <tbody>
                <tr><td class="mono">battle:roster:{battleId}</td><td>pid1,pid2,...</td><td>花名册（顺序即 spawn_index 序）</td></tr>
                <tr><td class="mono">battle:auth:{battleId}</td><td>bk{id}-{rand}</td><td>进入校验码</td></tr>
                <tr><td class="mono">battle:level:{battleId}</td><td>levelId,difficulty</td><td>关卡/难度</td></tr>
                <tr><td class="mono">battle:equip:{battleId}</td><td>pid:primaryGun:g0..g5;...</td><td>玩家初始装备</td></tr>
                <tr><td class="mono">battle:replay:{battleId}</td><td>win|finalHash|frame1|...</td><td>逐帧输入回放</td></tr>
                <tr><td class="mono">battle:result:{battleId}</td><td>win|hash|frames|ts|players|roster|duration|kills</td><td>战报摘要（battles.php 读取）</td></tr>
                <tr><td class="mono">battle:index</td><td>ZADD score=ts member=battleId</td><td>战报时间倒序索引</td></tr>
                <tr><td class="mono">battle:spectator:{battleId}</td><td>pid,...</td><td>观战者名单</td></tr>
            </tbody>
        </table></div>
    </div>
</div>

<?php layout_footer(); ?>
