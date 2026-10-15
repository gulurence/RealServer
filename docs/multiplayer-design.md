# RealServer 联网对战改造设计方案

> 目标：把单机 Cocos2d-x 射击游戏（`client/soulofgun/samples/HelloCpp`）接入 RealServer，
> 实现「登录 → 创建/选择角色 → 大厅 → 组队房间 → 开始战斗 → 帧同步对战 → 结算退房」完整流程。
> 本文为前期梳理，给出客户端改动点、服务器新增功能、协议定义与分阶段实施计划。
>
> ⚠️ **现状偏离提示（2026-10-13）**：本文为设计期规划，进程/Actor 承载关系已随实现演进，以
> `engine/xCluster/xProcessDefine.h` 与 `docs/web/框架/01-类型系统与进程划分.md` 为准：**Room(103) 现由 Game(201) 承载**、
> **Battle(203)/Spectator(204) 现由 Battle 进程(202) 承载**、**World 进程/Actor 已移除**。下文「Room→Global / Battle→Game / Spectator→Global / World」均为历史设计。

---

## 0. 结论速览

| 维度 | 现状 | 目标 |
|---|---|---|
| 客户端 | 单机 PvE，60FPS 渲染驱动、`rand()`/墙钟、本地存档 | 联网，固定步长 + 确定性随机 + 帧计数 + 帧同步 |
| 服务器 | 已有 Actor/RPC/Gate/登录选角雏形 | 新增 Friend/Room/Chat/Battle 四个 Actor + Battle/Lobby 进程 + 帧同步 |
| 关键缺口 | **无「ActorID→进程」索引**，`FindActorRemote` 每次重选最低负载进程 | 新增 `cluster:actor:{type}:{id}` Redis 注册 |
| 帧同步 | 无 | 守望先锋式：客户端/服务器**共用同一份**确定性模拟核心（`shared/battlecore/`），BattleActor 权威模拟 + 客户端预测/和解（参考 Quake3e/龙谷） |

---

## 1. 现状分析

### 1.1 客户端（单机战斗）

Cocos2d-x v2，战斗主循环是 `StageScene::update()` → `do_GameRunning()`。
能复用**全部战斗规则/数值/AI/波次/地图碰撞/表现动画**，但以下 6 处必须改：

| # | 维度 | 现状（file:line） | 改造 |
|---|---|---|---|
| 1 | 帧驱动 | `StageScene.cpp:206/774` 用渲染 `dt`；`Bullet.cpp:363/864` 位移无 dt（`+= cos/sin*speed`） | 固定步长累加器（逻辑帧 20Hz 固定 dt），Bullet 全改 dt |
| 2 | 随机 | `Defines.h:26` `rand()%x`；`AppDelegate.cpp:85` 墙钟 seed；`Sp01.cpp/Hero.cpp/Bullet.cpp/WeaponManager.cpp/Sp01Boss.cpp` 大量 `rand/CCRANDOM_0_1` | 换成「服务器下发种子 + 确定性 PRNG」 |
| 3 | 时间 | `WeaponManager.cpp:92/94/118` 射击 CD 用 `gettimeofdayCocos2d`；`Sp01.cpp:22/992/1027`、`Tools.cpp:1517` 用 `millisecondNow()` | 换成逻辑帧计数 |
| 4 | 输入 | `HudLayer.cpp:112-113` `static bool m_bIsHeld/m_bIsShooting` 共享缓存；`HudLayer.cpp:305-608` 双摇杆触摸 | 改成**每帧输入指令序列**（moveDir/shootDir/fire/skill） |
| 5 | 状态 | Hero/Sp01/Sp01Boss/WeaponManager/Bullet/Prop 的 CCNode 共享可变状态 | 可序列化快照 + 固定 CCArray/字典遍历顺序 |
| 6 | 结算/存档 | `StageScene.cpp:534-588/571` 胜利即写本地存档/解锁；`GameRecord.cpp` 金币钻石装备关卡任务 | 结算去副作用 → 胜利帧上报服务器；`GameRecord` 移到服务器账号存档 |

> 入口调用链：`UIMainMenuLayer → LoadingLayer(case0 建 StageScene → case20 建地图/Hero/Sp01Boss → case60 建 Hud) → GameScene`。
> 胜负判定：胜利 `Sp01Boss.cpp:256-268`（最后一波清空）→ `do_GameWin`；失败 `Hero.cpp:694` HP≤0 → `do_GameFailed`。

### 1.2 服务器（现状与缺口）

已有：Actor 模型、进程类型、RPC（TCP）、Gate 网络层、登录/选角（`si_login_rpc` → Login 进程）、服务发现（Redis 进程注册）。

**关键缺口（必须优先解决）**：目前 Redis 只注册**进程**（`cluster:process:{type}:{id}`），**没有 Actor 级位置索引**。
`CActorLoadManager::FindActorRemote`（`CActorLoadManager.cpp:82-105`）每次都重新 `PickProcessForActor`（选当前最低负载进程），
**不记忆该 Actor 当初创建在哪台进程**。负载变化时 `FindActor` 会 miss。
帧同步战斗 / 房间 / 好友都必须「按 ActorID 精确定位进程」，所以第一步要补 `cluster:actor:{type}:{id} → processId` 的注册/注销。

---

## 2. 目标架构

### 2.1 进程类型（EProcessType，`engine/xCluster/xProcessDefine.h:33-40`）

| 值 | 进程 | 承载 Actor | 端口段 |
|---|---|---|---|
| 100 | Gate | GateActor（单例） | 50551-50600 |
| 101 | Login | LoginActor（单例） | 50601-50650 |
| **102** | **Global**（新增） | FriendActor + RoomActor + ChatActor + SpectatorActor | 50751-50800 |
| 201 | Game | PlayerActor + BattleActor（+ WorldActor 备用） | 50651-50700 |
| 202 | World | WorldActor（**备用进程**，WorldActor 实际由 Game 承载） | 50701-50750 |

> 决策已定（用户重新定义进程）：进程分两大类——**网关/登录接入类**（Gate、Login）、**业务类**（Global 全局、Game 游戏、World 备用）。
> 全局系统 Actor（Friend/Room/Chat/Spectator）同驻一个 **Global 进程**；游戏 Actor（Player/Battle）同驻 **Game 进程**。
> 原「社交单例各自独立进程」方案已废弃。进程类型值 == 承载的首个 Actor 值（102/201），与 Gate/Login 约定一致。

### 2.2 Actor 类型（EActorType，`xProcessDefine.h:43-50`）

| 值 | Actor | 承载进程 | 实例形态 | 脚本目录 |
|---|---|---|---|---|
| 100 | Gate | Gate | 单例 | gate |
| 101 | Login | Login | 单例 | login |
| **102** | **Friend** | Global | 单例 | friend |
| **103** | **Room** | Global | 单例 | room |
| **104** | **Chat** | Global | 单例 | chat |
| 201 | Player | Game | 多实例（一玩家一个） | player |
| 202 | World | Game（备用） | 单例 | world |
| **203** | **Battle** | Game | 多实例（一战斗房一个） | battle |
| **204** | **Spectator** | Global | 单例 | spectator |
| **300** | **Log** | 独立程序（日志进程） | 单例 | —（独立程序，非 Lua 脚本） |

### 2.3 通信拓扑

```
客户端 ──TCP── Gate(GateActor) ──RPC── Login / Global(Friend/Room/Chat/Spectator) / Game(Player/Battle)
```

- 客户端所有消息统一发 Gate，`CNetGateway` 解析 `big_type`/`small_type` 得 `msgId=(big<<16)|small`。
- Gate 按 `big_type` 把消息 RPC 转发给对应 Actor 所在进程，回包再 `si_net_send(cid, ...)` 回客户端。
- 服务器主动推送（房间列表刷新、聊天广播、帧命令、战斗结束）也走 `si_net_send` 到对应 CID。
- **进程间 Actor 通信全部走 asio + lambda 实现的 RPC**（TCP，超时返回 TimeOut）；路由用全局 ActorID（§2.5）。

### 2.4 服务器实现语言分工（C++ vs Lua）

**原则：服务器业务逻辑除「战斗帧同步核心」外全部用 Lua 实现；C++ 只做框架/基础设施 + 战斗帧同步核心。**

| 类别 | 语言 | 内容 |
|---|---|---|
| 框架/基础设施 | **C++** | 枚举（进程/Actor 类型）、`cluster:actor` 索引、进程启动、链路管理、RPC 帧、proto 生成物、SWIG 绑定 |
| 战斗帧同步核心 | **C++** | `shared/battlecore/`（确定性模拟）+ `engine/xActor/Battle/`（服务器侧封装）：帧号推进、确定性模拟、帧命令、快照 checkpoint、一致性哈希 —— **服务器唯一用 C++ 写的业务逻辑** |
| 登录/选角 | Lua | 现有 `login`/`player`（保持不动） |
| 好友 FriendActor | **Lua** | 加好友/删好友/列表/邀请战斗（`script/actors/friend/`） |
| 房间 RoomActor | **Lua** | 建房/列表/加入/离开/踢人/开始战斗/成员态管理（`script/actors/room/`） |
| 聊天 ChatActor | **Lua** | 房间频道 + 战斗频道 收发/转发/过滤（`script/actors/chat/`） |
| BattleActor 普通逻辑 | **Lua** | 房间成员管理、出生点分配、结算流程、掉线/重连编排、回放数据落库（`script/actors/battle/`） |
| SpectatorActor 观战 | **Lua** | 观战席位管理、订阅战斗帧流、向观战客户端转发帧流（`script/actors/spectator/`） |

> 与 CLAUDE.md 红线一致：「业务逻辑写在 Lua，C++ 只做框架与基础设施」；唯一例外是帧同步核心（用户点名 C++，为性能 + 与客户端共用 `BattleCore`）。

### 2.5 进程间通信与全局 ID 编码

**通信**：进程间 Actor 交互全部走 **asio + lambda 实现的 RPC**（TCP，超时返回 TimeOut）。现有代码是 gRPC，需**全量迁移到 asio**（M0 基建项）。

**全局 ID（64 位，自描述）**：

```
ProcessID[64位] = 0[高24位，暂保留] + MachineID[24位] + PID[16位]   // 前 24 位暂置 0
ActorID[64位]   = MachineID[24位] + PID[16位] + ActorIndex[24位]   // 24+16+24=64
```

| 段 | 位数 | 来源 |
|---|---|---|
| MachineID | 24 | 进程启动时由 Redis 管理、物理机唯一 ID 派生 |
| PID | 16 | 进程启动时由操作系统分配 |
| ActorIndex | 24 | 进程内管理，自增、从 1 开始 |

- ActorID 全局唯一且**自描述**：从 ActorID 解析 `MachineID + PID` 即定位进程，无需再查「Actor→进程」索引即可路由。
- **Actor 间消息路由规则**（例：玩家 100 发消息给玩家 200）：
  1. 用目标 `PlayerID` 查 Redis 得到其 `ActorID`（`player:{playerId} → actorId`）；
  2. 用 `ActorID` 路由（`MachineID+PID` 定位进程、`ActorIndex` 定位进程内 Actor）；
  3. 优先用消息里带的 / 本地缓存的 `ActorID`；若 `ActorID` 失效（目标离线/迁移）→ **回 Redis 重取最新 ActorID**。

### 2.6 类型枚举清单与分离（检查结果）

三类「类型」枚举按前缀分离清晰：

| 枚举 | 前缀 | 位置 | 取值 |
|---|---|---|---|
| 进程类型 `EProcessType` | `P` | `xProcessDefine.h` | `PGate=100, PLogin=101, PGlobal=102, PGame=201, PWorld=202` |
| Actor 类型 `EActorType` | `A` | `xProcessDefine.h` | `AGate=100, ALogin=101, AFriend=102, ARoom=103, AChat=104, APlayer=201, AWorld=202, ABattle=203, ASpectator=204, ALog=300` |
| 协议路由 `ActorType` | `AT_` | `msg_id.proto` | `AT_GATE=100 … AT_SPECTATOR=204`（与业务 Actor 一一对应，作 `big_type`） |
| 进程状态 `EProcessState` | — | `xProcessDefine.h` | `Starting/Init/Running/Paused/Stopping/Shutdown`（独立，不参与路由） |

> 值域重叠说明：`102` 进程层是 `Global`、Actor 层是 `Friend`；`201` 进程层是 `Game`、Actor 层是 `Player`——沿用现有「值域相同、语义不同」约定（靠 `P`/`A`/`AT_` 前缀区分）。
> 若希望彻底消除值域重叠（如进程用 1xx/2xx、Actor 用 1xxx），需你确认是否改值。

### 2.7 编码/命名风格约定（已定）

统一采用「**沿用现有 C++ 风格 + Lua 全 snake_case**」：

| 语言 | 规则 |
|---|---|
| C++ 类 | 框架基类 `x*`；管理器/核心类 `C*`；普通结构体无前缀 PascalCase |
| C++ 方法 | PascalCase（`GetHostedActorTypes`） |
| C++ 成员 | `m_` + camelCase（可带类型提示：`m_mapInstances`、`m_i32Seed`） |
| C++ 参数/局部 | 匈牙利前缀（`i32Frame`、`i64PlayerId`、`pInput`） |
| C++ 枚举 | `E枚举名_值`（`EProcessType_PGate`） |
| C++ 整数 | `int8/int16/int32/int64/uint8/uint16/uint32/uint64` 别名（禁裸 int/long） |
| C++ 日志 | `XLOG/XERR/XWRN`（集群 `XCLOG/XCERR/XCWRN`），禁 cout/printf |
| Lua | 全 **snake_case**（`on_install`、`on_create`、`data_load`、`on_event`）；类 `xEngine.Class.define("Xxx")` |
| Proto | `PB` 前缀消息 + 大写枚举 + snake_case 字段（生态惯例，不动） |
| 注释 | 中文为主 |

> 共享 `BattleCore`（`shared/battlecore/`）沿用同一套 C++ 风格（`CBattleCore` / `m_` / 匈牙利），跨客户端/服务器一致。

---

## 3. 登录流程（4.1）

沿用现有 `gate → login` 链路，客户端补齐真实登录 UI：

1. WebPlatform 建号（已有 `api/account.php`）+ 登录拿 Session token（已有 `api/login.php`）。
2. 客户端 TCP 连 Gate，发 `PBMsgLoginReq{token}`（已有 `login.proto`）。
3. Gate `si_login_rpc(LOGIN, payload)` → Login 校验 Session → 回 `PBMsgLoginRsp{account, roles[]}`。
4. 客户端展示角色列表 → 选角 `PBMsgSelectRoleReq` → Login `balanceCreate(AT_PLAYER, roleId)` 在 Game 建 PlayerActor → 回 `PBMsgSelectRoleRsp{player}`。
5. 客户端进入大厅（大厅 UI 是新增）。

**客户端改动**：新增登录/选角 UI（可复用现有 `UIChoosePlayer` 风格 + `UIMainMenuLayer`），新增 TCP 网络层（连接/收发/粘包拆包）。

**服务器改动**：基本无需改动（登录/选角已完成），仅把选角成功后「进入大厅」的推送补上。

---

## 4. 好友系统 FriendActor（4.2）

仿王者荣耀：加好友 / 同意 / 拒绝 / 删除 / 好友列表 / 在线状态 / 邀请组队。

**单例 FriendActor（id=0）**，Lua 业务（`script/actors/friend/`），数据走 `CRecordMgr`（Redis 缓存 + DB 落库）。

协议（`msg/friend.proto`，`big_type=AT_FRIEND=102`）：

```
FriendMsgId { FRIEND_ADD_REQ=1, ADD_RSP=2, ADD_APPLY_NTF=3, ADD_ACCEPT_REQ=4, ADD_ACCEPT_RSP=5,
              FRIEND_LIST_REQ=6, FRIEND_LIST_RSP=7, FRIEND_DEL_REQ=8, FRIEND_DEL_RSP=9,
              FRIEND_ONLINE_NTF=10, FRIEND_INVITE_REQ=11, FRIEND_INVITE_NTF=12 }
```

- 加好友：`ADD_REQ{target_player_id}` → FriendActor 写「申请」，`ADD_APPLY_NTF` 推给被申请人 → 被申请人 `ADD_ACCEPT_REQ`。
- 邀请组队：`FRIEND_INVITE_REQ{friend_id}` → FriendActor 校验好友关系 → `FRIEND_INVITE_NTF` 推给好友，好友点同意后由 RoomActor 拉进房间。
- 在线状态：PlayerActor 登录/登出时通知 FriendActor 更新，`FRIEND_ONLINE_NTF` 推给其好友。

---

## 5. 组队房间 + 聊天（4.3）

### 5.1 RoomActor（单例，`big_type=AT_ROOM=103`）

仿王者荣耀组队：创建 / 房间列表 / 加入 / 离开 / 准备 / 踢人 / 转让房主 / 开始游戏。

协议（`msg/room.proto`）：

```
RoomMsgId { ROOM_CREATE_REQ=1, CREATE_RSP=2, ROOM_LIST_REQ=3, ROOM_LIST_RSP=4,
            ROOM_JOIN_REQ=5, JOIN_RSP=6, ROOM_LEAVE_REQ=7, LEAVE_RSP=8,
            ROOM_READY_REQ=9, READY_RSP=10, ROOM_KICK_REQ=11, KICK_RSP=12,
            ROOM_TRANSFER_REQ=13, ROOM_START_REQ=14, START_RSP=15,
            ROOM_INFO_NTF=16, ROOM_MEMBER_NTF=17 }
```

- RoomActor 维护 `roomId → {房主、成员列表、准备态、房间状态}`，全部房间由一个 Actor 管（内存态 + 可选 Redis 备份）。
- `ROOM_CREATE_REQ` → 分配 `roomId`，`CREATE_RSP{room_id, 成员}`；`ROOM_LIST_RSP` 返回可加入房间摘要。
- 状态变化（进房/离房/准备/踢人/转让）→ `ROOM_MEMBER_NTF` 广播给房内所有成员。
- `ROOM_START_REQ`（仅房主）→ 校验全员已准备 → 调 Battle 进程创建 BattleActor（见 §6）。

### 5.2 ChatActor（单例，`big_type=AT_CHAT=104`）

频道分两类：**房间频道** + **战斗频道**（4.6）。

协议（`msg/chat.proto`）：

```
ChatMsgId { CHAT_SEND_REQ=1, CHAT_SEND_RSP=2, CHAT_NTF=3, CHAT_HISTORY_REQ=4, CHAT_HISTORY_RSP=5 }
```

- `CHAT_SEND_REQ{channel_type, channel_id, content}` → ChatActor 广播 `CHAT_NTF` 给该频道所有人。
- `channel_type=1` 房间频道（channel_id=roomId），`channel_type=2` 战斗频道（channel_id=battleId）。
- ChatActor 记录频道成员（谁在哪个房间/战斗），广播时经 Gate 下发。

---

## 6. 开始并进入战斗流程（4.4）

```
房主点开始
  → RoomActor(ROOM_START_REQ) 校验全员准备
  → RoomActor RPC 到 Battle 进程 CreateActorRemote(AT_BATTLE, battleId)
  → BattleActor 创建成功 → 注册 cluster:actor:203:{battleId}→processId
  → 回 RoomActor：battleId + 验证Key(authKey)
  → RoomActor 广播 ROOM_START_RSP{battle_id, auth_key, 成员} 给房内全员
客户端收到 START_RSP
  → 用 battle_id + auth_key 发 BATTLE_ENTER_REQ 给 BattleActor（经 Gate 转发）
  → BattleActor 校验 auth_key → 回 BATTLE_ENTER_RSP{种子, 玩家列表, 出生点, 关卡/难度}
  → 客户端加载战斗场景（复用 LoadingLayer 构建 StageScene，建英雄/核心），但**帧时钟先不起跑**
  → 全员（花名册）都进入后，BattleActor 广播 BATTLE_START_NTF{battle_id, seed}
  → 各客户端收到 START_NTF 后**统一从帧 0 起跑**（起始帧由服务器下发，见 §7.1 同步起点）
```

对应 4.10 预期：服务器创建 BattleActor，返回 ActorID + 验证 Key；客户端凭 Key 进入战斗房间、加载场景、开始帧同步。

### 6.1 观战流程（新增，见决策 §16-9）

观战玩家从「战斗房间/好友战斗」入口进入，走独立 SpectatorActor 分流，**只读帧流、不参与模拟**：

```
观战玩家发 SPECTATE_ENTER_REQ{battle_id}
  → Gate → SpectatorActor(204)
  → 校验该战斗可被观战 → 向 BattleActor 登记观战席位（订阅帧流）
  → BattleActor 每帧额外把 FRAME_NTF 转发一份给 SpectatorActor
  → SpectatorActor 先补一帧 SPECTATE_SNAPSHOT_NTF（快照对齐）→ 再转发 SPECTATE_FRAME_NTF 追帧
  → 观战客户端用「观战 UI」播放（只读，不上报 INPUT）
观战退出：SPECTATE_LEAVE_REQ → 退订席位 → LEAVE_RSP
战斗结束：SPECTATE_END_NTF → 观战客户端退回大厅
```

> 观战复用 BattleCore 的确定性模拟 + 快照：观战客户端本地跑同一份 `BattleCore` 播放帧流（与 §7.6 共享核心一致），进入时用快照对齐，之后按帧流追帧。

---

## 7. 帧同步战斗（4.5 / 4.9 / 4.12）

### 7.1 模型：守望先锋式确定性同步（Overwatch 模式）

采用守望先锋（Overwatch）的联网模型 —— **确定性模拟 + 服务器权威 + 客户端预测与和解（prediction + reconciliation）**，而非纯锁步中继：

- **逻辑帧率：20Hz（固定 50ms/帧）**。
- 服务器与客户端跑**同一套确定性模拟**（固定步长、确定性随机、确定性物理）——这份模拟逻辑做成客户端/服务器**共用同一份代码**（见 §7.6）。
- **BattleActor = 权威服务器**：C++ 核心跑完整权威模拟，负责帧号推进、输入收集、权威校验、帧命令/快照广播、胜负判定。
- **客户端 = 本地预测 + 和解**：本地同样跑确定性模拟做预测，本地输入即时生效；收到服务器帧命令后，若与本地预测不一致则「回滚重放（rollback）」纠正。
- **输入缓冲**：输入按固定延迟生效（所有客户端帧对齐一致）。
- **同步起点（起始帧由服务器下发）**：起始帧（帧 0）**由服务器下发**——BattleActor 在花名册全员进入后广播 `BATTLE_START_NTF`，各客户端收到后才统一把帧时钟从 0 起跑。客户端在 `ENTER_RSP` 阶段只建英雄/确定性核心（`m_netStarted=false` 待命渲染），收到 `START_NTF` 再 `ResetBattleClock(0)` 起跑。若各端各自在 `ENTER_RSP` 就起跑，加载耗时不同会让同一帧号对应不同墙钟时刻的输入，导致远端英雄表现错乱。
- **快照分支**：模拟状态按间隔打快照（checkpoint），可从任意快照分支重放——既用于客户端回滚和解，也用于**战斗回放**（§7.5）。

> 参考实现：
> - **服务器帧同步**：参考 **Quake3e**（`usercmd_t` 命令处理 / `snapshot_t` 快照 / 固定 tick 的 `SV_Frame` 主循环 / 客户端预测共享的 `pmove` / delta 压缩）。
> - **客户端帧同步**：参考**端游 3D 版「龙谷」**的客户端预测 + 插值 + 和解实现。

### 7.2 BattleActor 职责划分（4.12：核心 C++，普通逻辑 Lua）

| 层 | 职责 | 语言 |
|---|---|---|
| 帧同步核心 | 帧号推进、输入收集/排序、确定性模拟、帧命令序列化、权威校验、**快照 checkpoint / 回放分支、一致性校验（状态哈希比对）** | **C++**（`engine/xActor/Battle/` 服务器侧封装，复用 `shared/battlecore/` 共享核心，见 §7.6） |
| 业务逻辑 | 房间成员管理、出生点分配、结算流程、掉线/重连、回放数据落库 | Lua（`script/actors/battle/`） |

**C++ 核心对 Lua 只暴露「纯模拟接口」，流程编排由 Lua 调用**（核心不含任何房间/网络/结算语义）：

| C++ 暴露接口 | 语义 |
|---|---|
| `New(seed, 成员列表)` | 建核心，注入种子 + 确定性 PRNG 初始化 |
| `SubmitInput(frame, playerId, PBPlayerInput)` | 按帧投递某玩家输入（Lua 收到 `BATTLE_INPUT_REQ` 后调用） |
| `Tick(frame)` → 帧命令 + 新状态 | 推进一个逻辑帧，跑确定性模拟，产出本帧帧命令 |
| `SerializeState()` / `DeserializeState()` | 全量快照序列化/恢复（checkpoint、断线重连、回放共用） |
| `HashState()` | 一致性校验哈希（比对客户端上报的 `PBBattleHashNtf`） |
| `CheckWin()` | 权威胜负判定结果 |

调用方向：

```
Lua（BattleActor）收到 BATTLE_INPUT_REQ ──→ core:SubmitInput(frame, pid, input)
        每逻辑帧 ──→ core:Tick(frame) ──→ 帧命令/快照 ──→ Lua 广播 BATTLE_FRAME_NTF / 落库 checkpoint
        结算判定 ──→ core:CheckWin() ──→ Lua 广播 BATTLE_END_NTF → 释放
        掉线重连 ──→ core:DeserializeState(快照) ──→ Lua 发 BATTLE_SNAPSHOT_NTF
```

> 边界：帧号推进、模拟、命令、快照、哈希在 C++；「何时建核心/何时 tick/何时结算/何时重连」等编排决策全在 Lua。符合 §2.4「服务器唯一 C++ 业务逻辑是帧同步核心」。

### 7.3 帧同步协议（`msg/battle.proto`，`big_type=AT_BATTLE=203`）

```
BattleMsgId { BATTLE_ENTER_REQ=1, ENTER_RSP=2, BATTLE_START_NTF=3,
              BATTLE_INPUT_REQ=4, BATTLE_FRAME_NTF=5, BATTLE_SNAPSHOT_NTF=6,
              BATTLE_END_NTF=7, BATTLE_LEAVE_REQ=8, LEAVE_RSP=9,
              BATTLE_HASH_NTF=10 }

PBPlayerInput { int64 player_id=1; uint32 move_dir=2; uint32 skill_id=4; bool fire=5; uint32 gun_type=6; sint32 shoot_dx=7; sint32 shoot_dy=8; }
PBBattleInputReq    { big_type, small_type, int64 battle_id=3, uint32 client_frame=4, PBPlayerInput input=5; }
PBBattleFrameNtf    { big_type, small_type, uint32 frame_id=3, repeated PBPlayerInput inputs=4; }
PBBattleSnapshotNtf { big_type, small_type, uint32 frame_id=3, bytes state=4; }   // 全量状态快照
PBBattleHashNtf     { big_type, small_type, uint32 frame_id=3, uint64 state_hash=4; }  // 客户端一致性校验上报
PBBattleEnterRsp    { big_type, small_type, int64 battle_id, uint32 seed, repeated PBPlayerInfo players, ... }
```

- 客户端每逻辑帧发 `BATTLE_INPUT_REQ`（也可多帧合并发）。
- BattleActor 每帧收集全员输入 → 跑完整权威模拟 → 组装 `BATTLE_FRAME_NTF{frame_id, inputs[]}`（**服务器权威帧命令，非纯中继**）→ 广播给全员。
- 客户端按 `frame_id` 顺序缓存/补帧播放，`seed` 用于初始化本地确定性 PRNG（与 §1.1 改造点 2 对应）；本地预测与服务器帧不一致时回滚重放。
- **一致性校验（做）**：客户端周期上报 `BATTLE_HASH_NTF{frame_id, state_hash}`，BattleActor 与权威状态哈希比对，不一致 → 回发 `BATTLE_SNAPSHOT_NTF` 强制该客户端对齐。
- **断线重连（帧快照实现）**：掉线玩家重连后，服务器补发一次全量快照 `BATTLE_SNAPSHOT_NTF`，客户端据此对齐，再从当前帧进入增量帧播放。

### 7.4 客户端战斗改造（4.9）

把 §1.1 的六件套落地，核心是新增一个「战斗逻辑层」与「表现层」解耦：

1. 新增 `BattleLogic`（即共享战斗核心 `BattleCore`，纯 C++，无 CCNode，见 §7.6）：持有确定性 PRNG、固定步长 `tick()`、实体数据（快照）、帧命令缓冲、**预测/回滚**（保存最近 N 帧快照，收到服务器帧命令不一致时回滚重放）；本地无服务器时直接跑它即为**单机模式**。
2. `StageScene/do_GameRunning` 从「渲染 dt 驱动」改为「`BattleLogic::tick()` 逻辑帧驱动 + 渲染层只负责展示当前快照」；本地输入即时预测生效，服务器帧到后做 reconciliation。
3. `Hero/Sp01/WeaponManager/Bullet` 的 `rand/gettimeofday/millisecondNow` 全部替换为逻辑帧 + PRNG。
4. `HudLayer` 输入改为生成 `PBPlayerInput`，逐帧上报。
5. 结算副作用（`StageScene.cpp:534-588` 存档/解锁）去掉，改为展示服务器下发的胜负结果。

### 7.5 战斗回放（快照分支）

确定性模拟天然可回放：记录 `(seed, 全员每帧输入流)` 即可完整重演整场战斗。为支持**任意时刻回放/跳转**，引入快照分支：

- **快照 checkpoint**：BattleActor 每 N 帧（如 30 帧 = 1.5s）保存一份完整模拟状态快照（含 PRNG 状态 + 所有实体状态）。
- **回放**：回放时从最近快照恢复 + 重放该快照之后的输入流，不必从头推演。
- **分支**：从任意快照可派生新分支（"如果当时没这么做"的推演 / 断线续战对齐）。
- **存储**：战斗结束后把 `(seed, 输入流, 快照序列)` 落库（Redis + DB），供回放系统拉取。

> 与 §7.3 的 `BATTLE_SNAPSHOT_NTF` 复用同一套快照序列化/恢复代码：断线重连 = 服务器发快照给客户端对齐；战斗回放 = 回放系统读快照 + 输入流重演。

### 7.6 战斗逻辑核心共享（渲染/逻辑分离，客户端服务器同源）

帧同步战斗逻辑（确定性模拟核心）做成**一份跨平台代码**，客户端与服务器**共用同一份**，从根上保证两端模拟一致：

- **分层**：
  - **共享逻辑核心 `BattleCore`**（纯 C++，无 CCNode、无引擎、无服务器依赖）：固定步长 `tick()`、确定性 PRNG、实体/碰撞/技能/波次/AI/胜负判定、状态快照序列化、帧命令打包/解析、回滚/重放。**客户端单机模式、客户端联网预测、服务器权威模拟，跑的都是这一个核心**。
  - **客户端表现层**（Cocos2d-x 独有）：CCNode 渲染、动画、音效、HudLayer 输入采集 → 只读 `BattleCore` 状态并展示，不参与逻辑。
  - **服务器 BattleActor 网络层**（RealServer 独有）：输入收集/排序、帧号推进、快照 checkpoint 落库、胜负广播、断线重连下发 —— 调用同一个 `BattleCore` 跑权威模拟。
- **代码位置**：`shared/battlecore/`（仓库根，与 `engine/`、`client/` 平级），客户端工程与服务器 CMake 各自 include 编译同一批源文件。
- **收益**：
  - 单机模式 = 客户端本地直接跑 `BattleCore`（离线可玩、可测试、可回放），无需服务器。
  - 联网模式 = 服务器权威 + 客户端预测，两端共用 `BattleCore`，避免「两套模拟实现」导致的漂移。
  - BattleActor 的 C++ 帧同步核心（§7.2）即 `BattleCore` 的服务器侧封装，不另写第二份逻辑。

---

## 8. 新增清单总览 + 协议流转（4.6 / 4.7 / 4.8 / 4.11）

### 8.1 新增进程与 Actor 系统清单

| 进程 | 承载 Actor | 形态 | 脚本目录 | proto |
|---|---|---|---|---|
| Global(102) | FriendActor(102) | 单例 | `script/actors/friend/` | friend.proto |
| Global(102) | RoomActor(103) | 单例 | `script/actors/room/` | room.proto |
| Global(102) | ChatActor(104) | 单例 | `script/actors/chat/` | chat.proto |
| Game(201) | BattleActor(203) | 多实例 | `script/actors/battle/` + `engine/xActor/Battle/`(C++) + `shared/battlecore/` | battle.proto |
| Global(102) | SpectatorActor(204) | 单例 | `script/actors/spectator/` | spectator.proto |

> 进程类型只新增 `PGlobal=102`（承载 Friend/Room/Chat/Spectator）；Battle 作为 Actor 挂到现有 `Game=201` 进程（与 Player 同进程）。
> `EActorType` 枚举新增 `AFriend=102/ARoom=103/AChat=104/ABattle=203/ASpectator=204`（`msg_id.proto` + `xProcessDefine.h` 同步加）。

### 8.2 新增协议清单

| proto | big_type | 消息（MsgId 枚举） | 条数 |
|---|---|---|---|
| friend.proto | AT_FRIEND=102 | ADD_REQ/RSP、ADD_APPLY_NTF、ADD_ACCEPT_REQ/RSP、LIST_REQ/RSP、DEL_REQ/RSP、ONLINE_NTF、INVITE_REQ/NTF | 11 |
| room.proto | AT_ROOM=103 | CREATE_REQ/RSP、LIST_REQ/RSP、JOIN_REQ/RSP、LEAVE_REQ/RSP、READY_REQ/RSP、KICK_REQ/RSP、TRANSFER_REQ、START_REQ/RSP、INFO_NTF、MEMBER_NTF | 17 |
| chat.proto | AT_CHAT=104 | SEND_REQ/RSP、NTF、HISTORY_REQ/RSP | 5 |
| battle.proto | AT_BATTLE=203 | ENTER_REQ/RSP、START_NTF、INPUT_REQ、FRAME_NTF、SNAPSHOT_NTF、END_NTF、LEAVE_REQ/RSP、HASH_NTF | 10 |
| spectator.proto | AT_SPECTATOR=204 | ENTER_REQ/RSP、LEAVE_REQ/RSP、SNAPSHOT_NTF、FRAME_NTF、END_NTF | 7 |

> 每条消息前两字段 `big_type=1 / small_type=2`，`msgId=(big_type<<16)|small_type`。合计 **50 条**客户端消息。

### 8.3 协议处理 Actor 与流转流程

**通用链路**：`客户端 → Gate(按 big_type 路由) → RPC 转发目标 Actor 所在进程 → Actor 处理 → 回包 si_net_send 回客户端`（主动推送 NTF 同走 `si_net_send`）。

| 协议 | 处理 Actor | 流转（请求路径 → 广播/推送） |
|---|---|---|
| login.proto | LoginActor(101) | `LOGIN_REQ → Gate → LoginActor 校验 token → RSP`；`SELECT_ROLE_REQ → balanceCreate PlayerActor(Game 进程) → RSP` |
| friend.proto | FriendActor(102) | `ADD_REQ → 写申请+DB → ADD_APPLY_NTF 推被申请人 → ACCEPT_REQ → 双方好友表写库 → ACCEPT_RSP`；PlayerActor 登录/登出 → RPC 通知 FriendActor → `ONLINE_NTF` 推好友；`INVITE_REQ → 校验好友关系 → INVITE_NTF 推好友` |
| room.proto | RoomActor(103) | `CREATE/JOIN/LEAVE/READY/KICK/TRANSFER_REQ → 改房间态 → MEMBER_NTF 广播房内`；`START_REQ(房主) → 校验全员准备 → RPC Battle 进程 CreateActorRemote(AT_BATTLE) → START_RSP{battle_id, auth_key}` |
| chat.proto | ChatActor(104) | `SEND_REQ → 按 channel 广播 CHAT_NTF 给频道全员 → SEND_RSP`；`HISTORY_REQ → HISTORY_RSP` |
| battle.proto | BattleActor(203) | `ENTER_REQ → 校验 auth_key → ENTER_RSP{seed, 花名册, 关卡/难度}`；全员就绪 → `START_NTF`（同步起点，客户端据此从帧 0 起跑）；每帧 `INPUT_REQ → SubmitInput → Tick → FRAME_NTF 广播`；周期 `HASH_NTF` 一致性校验；`LEAVE_REQ → 标记离场 → LEAVE_RSP`；胜负 → `END_NTF` 广播 |
| spectator.proto | SpectatorActor(204) | `ENTER_REQ{battle_id} → 校验可观战 → 向 BattleActor 登记席位 → SNAPSHOT_NTF 快照对齐 → 转发 FRAME_NTF`；`LEAVE_REQ → 退订席位 → LEAVE_RSP`；战斗结束 → `END_NTF` |

> 跨进程 RPC 关键路径：Gate → Login/Global(Friend/Room/Chat)/Game(Battle) 按 `big_type` 转发；Room(Global) → Battle(Game) 用 `CreateActorRemote`；Player → Friend 用在线状态通知；回包原路返回（超时 RPC 返回 TimeOut，见 §9）。

---

## 9. Actor 寻址与消息收发机制（4.11）

需求 4.11 = 创建（负载均衡）+ 交互（按 ActorID 精确寻址）+ RPC + 超时。全局 ID 编码见 §2.5，这里补交互寻址。

**补齐方案**（负载均衡只用于创建；交互靠「PlayerID → ActorID → 路由 + 本地缓存」）：

1. **创建（负载均衡）**：`CreateActorRemote` 用 `PickProcessForActor` 选最低负载进程创建（负载均衡仅在此生效）；创建成功后写 Redis 映射 `player:{playerId} → actorId`（业务 ID → 全局 ActorID）。单例 `install` 时同样注册。
2. **交互（PlayerID → ActorID → 路由）**：Actor 间交互先查**本地缓存** `map<playerId, actorId>`；命中 → 用 ActorID 直接路由（解析 `MachineID+PID` 定位进程）。缓存未命中 / ActorID 失效（目标离线/迁移）→ **回查 Redis** 拿最新 ActorID → **更新本地缓存** → 再路由。Redis 也查不到 → 目标不存在（报错或按需新建）。
3. **注销与失效**：`UninstallActorRemote` / Actor 卸载时删除 Redis 映射并清本地缓存；进程崩溃靠 TTL + 心跳兜底，本地缓存条目带失效时间 + 目标进程存活校验。
4. **消息收发**：目标进程收到 RPC → `CActorMgr::Find(type, ActorIndex)` 定位 Actor → 投递消息队列 → 处理 → 结果原路 RPC 返回；超时 `CallSyncRaw` 返回 `code=2`（已有）。

这套完成后，房间/好友/战斗「按 ActorID 精确寻址」全部依赖它。

---

## 10. 客户端改动清单（4.13）

C++ 侧（复用现有资源拼 UI，后续再调）：

1. **网络层**（新增）：TCP 连接 Gate、`[uint32 len][protobuf body]` 收发、粘包拆包、重连。
2. **协议层**（新增）：protobuf 编解码（login/room/chat/battle/friend）。
3. **登录/选角 UI**（新增）：登录、角色列表、选角。
4. **大厅 UI**（新增）：房间列表、创建房间、加入房间、聊天框。
5. **战斗重构**（改造）：§7.4 的 `BattleLogic` 分层（渲染/逻辑分离，逻辑用共享 `BattleCore`）+ 帧同步播放；保留本地单机模式（无服务器时直跑 `BattleCore`）。
6. **结算/回大厅**（改造）：战斗结束收 `BATTLE_END_NTF` → 展示结算 → 退回大厅。
7. **存档**（移除）：`GameRecord` 本地存档改为服务器下发。

---

## 11. 战斗结束收尾（4.14）

```
BattleActor 判定胜负（权威）
  → 广播 BATTLE_END_NTF{win_side, 统计} 给全员
  → 客户端展示结算 → 退回大厅（RoomActor 更新房间态：成员回房间或解散）
  → BattleActor 停帧循环 → 落库回放数据 (seed, 输入流, 快照序列) → UninstallActorRemote（删 cluster:actor:203:{battleId}）
  → 房间可再来一局（ROOM_START_REQ 再创 BattleActor）
```

- 中途退出：`BATTLE_LEAVE_REQ` → BattleActor 标记该玩家离场 → 广播；全员退出后释放 BattleActor。
- 断线重连：掉线玩家凭 battleId + authKey 重连 → 服务器发 `BATTLE_SNAPSHOT_NTF` 快照对齐 → 继续帧同步。
- PlayerActor 全程保持在线（战斗是 PlayerActor 的一次「进入战斗态」），战斗结束回到大厅态。

---

## 12. Actor 两类型划分（4.15）

| 类型 | 例子 | 生命周期 |
|---|---|---|
| 单例 | RoomActor / ChatActor / FriendActor（一个管所有房间/频道/好友） | 进程启动 install，常驻 |
| 多实例 | BattleActor（一战斗房一个）、PlayerActor（一玩家一个） | 动态 CreateActorRemote / UninstallActorRemote |

---

## 13. 进程类型与负载（4.16）

- 进程类型决定「可负载哪些 Actor」，映射在 `GetHostedActorTypes`（当前硬编码 switch）。
- **改造**：把该映射抽成 XML 配置（`server.xml` 的 `<process type>` 段加 `<hosted-actors>` 列表），启动时读入，替代硬编码 switch；同时新增 `player:{id} → actorId` 索引支撑动态负载与路由。
- 目标映射：Gate→GateActor；Login→LoginActor；**Global→Friend/Room/Chat/Spectator**；**Game→Player/Battle**（+World 备用）。

---

## 14. 服务器新增功能 / 最小文件集合

> 语言分工见 §2.4：**除「战斗帧同步核心」外全部 Lua**。下列 C++ 项 = 框架/基础设施 + 战斗核心封装。

**C++ 层（框架/基础设施 + 战斗核心）**：
1. `engine/xCluster/xProcessDefine.h` — `EProcessType` 加 `PGlobal=102`；`EActorType` 加 Friend/Room/Chat/Battle/Spectator（102/103/104/203/204）；补 `GetHostedActorTypes / ActorTypeToProcessType / ActorTypeDirName / ProcessTypeName / GetBasePortForType` 五个 switch；加全局 ActorID 编解码（§2.5）。
2. `engine/xCluster/CActorLoadManager.cpp` — **新增 `player:{playerId} → actorId` 注册/寻址/注销**（§9，优先级最高）；全局 ActorID 解析路由。
3. `shared/battlecore/`（新增，仓库根，客户端/服务器共用）— 确定性模拟核心（纯 C++：固定步长 tick、确定性 PRNG、实体/碰撞/技能/胜负、快照序列化、帧命令、回滚/重放）。`engine/xActor/Battle/`（新增）— BattleActor 服务器侧封装（帧号/输入收集/帧命令/快照 checkpoint 落库/一致性校验哈希），复用 `BattleCore`。
4. `engine/xEnv/BaseEnv.h` + `engine/xEnv/ProcessEnv.cpp` — `GetKernelModulesForType` / `RegisterActorDeps` 加 Global 进程 + Friend/Room/Chat/Battle/Spectator Actor 分支。
5. `engine/xCluster/xLinkManager.cpp` — 默认链接列表加 Global。
6. `engine/xRpc/` — **从 gRPC 迁移到 asio + lambda RPC**（M0 基建，全量替换）；`ActorRpcHandlers.cpp` 若需 Battle 专用跨进程 RPC 方法则加。

**配置/启动**：
7. `config/server.xml` — 新增 `<process type="102">`(Global，hosted-actors=Friend/Room/Chat/Spectator)；Game 进程 hosted-actors 加 Battle。
8. `launch_all.ps1` / 新增 `start_global.sh`。

**协议**：
9. `source/proto-def/msg/msg_id.proto` — `ActorType` 枚举加 102/103/104/203/204。
10. `source/proto-def/msg/{friend,room,chat,battle,spectator}.proto`（新增）。
11. `source/proto-def/make.bat` — 追加 5 个 proto 生成命令。
12. `engine/luacore/luacore.i` — 新增 `si_pb_*` 编解码绑定（按需）。

**Lua 层**：
13. `script/common/actor_type.lua` + `script/common/msg_id.lua` — 补新类型与消息 handler 登记。
14. `script/actors/{friend,room,chat,battle,spectator}/`（新增，各含 `<actor>.lua` + `net_msg/` + `on_rpc/` + `on_event/` + `include.lua`）+ 三平台 `init.lua`。

**日志（Nats）**：
15. 新增**单独程序**日志进程，负载 Log Actor（`ALog=300`，Nats 消费者 → 落盘）；日志前缀带 `MachineID/ProcessID/ActorID/PlayerID`。
16. 分文件存储：按 `物理机ID + 进程ID` 分文件，ProcessID 转 string。

---

## 15. 分阶段实施计划

| 阶段 | 内容 | 验收 |
|---|---|---|
| **M0 基础设施** | 补 `cluster:actor` 索引；加 Actor/进程枚举（Friend/Room/Chat/Battle）+ hosted-actors 配置；四类新进程可启动注册 | 能跨进程按 ID 精确 FindActor |
| **M1 登录闭环** | 客户端网络层 + 登录/选角 UI（接现有 gate→login） | 多人登录进大厅 |
| **M2 房间+聊天** | RoomActor + ChatActor + proto + 大厅 UI | 多人进同一房间、房间内聊天 |
| **M3 好友** | FriendActor + proto | 加好友/列表/邀请 |
| **M4 开始战斗** | BattleActor 创建/进入/开始 + 客户端战斗场景加载 | 房主开房，全员进战斗 |
| **M5 帧同步** | BattleActor C++ 帧同步核心（20Hz 权威模拟 + 快照 checkpoint + 一致性校验）+ 客户端 BattleLogic 重构 | 多人同屏实时对战 |
| **M6 收尾** | 胜负结算、退回大厅、BattleActor 释放、帧快照断线重连、战斗回放 | 打完回大厅可再来一局 + 可回放 |

---

## 16. 决策汇总（已定）

1. **进程部署（重新定义）**：进程分两大类——接入类（Gate 网关 / Login 登录）+ 业务类（Global 全局 / Game 游戏 / World 备用）。**Global 进程承载 Friend/Room/Chat/Spectator**；**Game 进程承载 Player/Battle**。新增 `Global=102` 进程；Battle 挂 Game、Spectator 挂 Global（均非独立进程）。
2. **逻辑帧率**：**20Hz**（固定 tick，50ms/帧；服务器权威模拟 + 客户端预测/和解）。
3. **帧同步模式**：守望先锋模式，BattleActor C++ 跑权威模拟（服务器权威 + 客户端预测/和解 + rollback），服务器参考 Quake3e（`usercmd_t`/`snapshot_t`/固定 tick `SV_Frame`/`pmove`/delta），客户端参考端游 3D 版龙谷。
4. **一致性校验**：**做**——客户端周期性上报状态哈希 `PBBattleHashNtf`，服务器比对帧内模拟状态，不一致则下发行快照校准/告警。
5. **掉线重连**：**用帧快照**——重连回战斗时服务器下发 `BATTLE_SNAPSHOT_NTF`（最近 checkpoint 快照 + 后续输入流），客户端快速追上当前帧。
6. **战斗逻辑核心共享**：帧同步逻辑（确定性模拟核心）做成客户端/服务器**共用同一份 C++ 代码**（`shared/battlecore/`）；客户端渲染/逻辑分离（渲染层客户端独有），逻辑核心的**单机模式、联网预测、服务器权威模拟三者跑同一份**，实现后直接给 BattleActor 使用，不另写第二套逻辑（§7.6）。
7. **战斗模式（PVP/PVE）**：创建房间时选战斗模式，带入 BattleActor 决定胜负/阵营规则。`ROOM_CREATE_REQ` 加 `mode` 字段（`MODE_PVE=1` 合作打波次/boss，`MODE_PVP=2` 玩家对抗）。
8. **战斗人数上限**：一个 BattleActor 最多 **5 人**。
9. **观战系统**：新增 `SpectatorActor(204)`（挂 Global 进程），从战斗房分流（§6.1）——观战玩家只读帧流、不参与模拟；进入时快照对齐 + 追帧，观战客户端复用 `BattleCore` 播放。
10. **角色选择**：**不允许重复选同一角色**（选角时校验已选角色集合）。
11. **邀请组队联动**：好友邀请同意后，若邀请人当前不在房间，**主动开房并把双方拉入**。
12. **房间可见性**：三态——公开 / 私密 / 密码房。`ROOM_CREATE_REQ` 加 `visibility` + 可选 `password`；`ROOM_LIST` 只列公开房 + 密码房（输密码可进），私密房仅邀请进入。
13. **断线重连窗口期**：**90 秒**（1.5 分钟）内可重连，超时判退出（离场）。
14. **聊天历史**：定期删除机制，**保留最近 20 条**消息。
15. **回放存储**：**全量回放**，保留 **一周（7 天）** 后删除。
16. **全局 ID 编码**：`ActorID[64] = MachineID[24] + PID[16] + ActorIndex[24]`；`ProcessID = MachineID[24] + PID[16]`（+24 位保留补 64）。MachineID 由 Redis 按物理机唯一 ID 派生、PID 由 OS 分配、ActorIndex 进程内自增从 1 开始。
17. **消息路由**：`PlayerID → Redis 查 ActorID → 按 ActorID 路由`；优先用消息中/本地缓存的 ActorID，失效回查 Redis（§2.5）。
18. **进程间通信**：全部 asio + lambda 实现 RPC（TCP，超时 TimeOut）；现有 gRPC 全量迁移到 asio（M0 基建项）。
19. **Nats 全局日志总线**：**单独程序**日志进程承载 Log Actor（`ALog=300`）消费落地；日志前缀含 MachineID/ProcessID/ActorID/PlayerID；按 `物理机ID + 进程ID` 分文件，ProcessID 转 string。
20. **类型枚举分离**：进程类型（`P` 前缀）/ Actor 类型（`A` 前缀）/ 协议路由（`AT_` 前缀）三类枚举分离；值域重叠（102=Global 进程 vs Friend Actor 等）沿用现有约定（§2.6）。
21. **编码/命名风格**：C++ 沿用现有（`C*`/`x*` 前缀 + `m_` 成员 + 匈牙利参数 + PascalCase + `E枚举_值` + int32 别名 + XLOG + 中文注释）；Lua 全 snake_case；Proto `PB` 前缀。共享 BattleCore 沿用同一套（§2.7）。

> 对应补充：快照分支（checkpoint every N 帧）同时支撑 §7.5 战斗回放与断线重连，二者复用同一套快照存储。

---

## 17. 原待定问题 → 已定

8 项待定问题已全部拍板，答案并入 §16 第 7-15 条：

1. 对战模式 → 建房选 PVP/PVE（§16-7）
2. 人数上限 → 5 人 + 新增观战系统（§16-8、§16-9）
3. 角色选择 → 不允许重复（§16-10）
4. 邀请联动 → 主动开房拉人（§16-11）
5. 房间可见性 → 公开/私密/密码三态（§16-12）
6. 断线重连 → 90 秒窗口（§16-13）
7. 聊天历史 → 保留最近 20 条（§16-14）
8. 回放存储 → 全量回放，保留一周（§16-15）

---

## 18. 全局日志总线（Nats）

Nats 作为全局日志总线，**专门的日志进程**负责消费并落地（分文件存储）。

- **日志前缀**：每条日志带完整前缀——`物理机ID(MachineID)、进程ID(ProcessID)、ActorID、PlayerID` 等，便于跨进程/跨机器串联定位。
- **分文件存储**：按 `物理机ID + 进程ID` 分文件；`进程ID` 转成 string 作为文件名/目录标识。
- **日志进程**：**单独程序**（独立可执行文件，非 RealServer 单二进制多进程），负载 **Log Actor**（`ALog=300`）——订阅 Nats → 分文件落盘。
