# 全局 ID 编码与 Actor 寻址

> M0 基础设施：64 位自描述全局 ID（`ActorID` / `ProcessID`）+ `player:{playerId} → actorId` 路由索引。
> 落地位置：`engine/xCluster/xActorId.h`（编解码）、`engine/xCluster/CPlayerActorIndex.*`（索引）、
> `engine/xActor/ActorMgr.cpp`（注册/注销钩子）、`engine/xCluster/CActorLoadManager.cpp`（初始化）。

## 1. 位布局

全局 ID 统一 `uint64` 存储，自描述（可直接解析出承载进程，无需再查「Actor → 进程」索引）。

```
ActorID[64位]   = MachineID[24位] + PID[16位] + ActorIndex[24位]

   bit63 ──────────────────────────────────────── bit0
   ┌───────────────┬──────────────┬──────────────────────┐
   │  MachineID    │     PID      │      ActorIndex      │
   │   [63..40]    │   [39..24]   │       [23..0]        │
   │    24 bits    │    16 bits   │        24 bits       │
   └───────────────┴──────────────┴──────────────────────┘

ProcessID[64位] = 0[高24位暂保留] + MachineID[24位] + PID[16位]

   bit63 ──────────────────────────────────────── bit0
   ┌───────────────┬──────────────┬──────────────────────┐
   │  0 (保留)     │  MachineID   │         PID          │
   │   [63..40]    │   [39..16]   │       [15..0]        │
   │    24 bits    │    24 bits   │        16 bits       │
   └───────────────┴──────────────┴──────────────────────┘
```

| 段 | 位数 | 来源 | 说明 |
|---|---|---|---|
| `MachineID` | 24 | 进程启动时由 Redis 按物理机唯一 ID 派生 | 标识一台物理机 |
| `PID` | 16 | 进程启动时由 OS 分配 | 标识同机一个进程 |
| `ActorIndex` | 24 | 进程内自增，从 1 开始 | 标识进程内一个 Actor 实例 |

> 注意：ActorID 与 ProcessID 的 `MachineID`/`PID` **位偏移不同**（ActorID 中 `MachineID` 在 bit40、
> `PID` 在 bit24；ProcessID 中 `MachineID` 在 bit16、`PID` 在 bit0）。二者转换必须走
> `ActorIdToProcessId` 显式重组，不能用简单移位截断。

## 2. 编解码（`engine/xCluster/xActorId.h`）

全部为 `inline` 纯函数，无状态，`uint64` 存储：

| 函数 | 语义 |
|---|---|
| `MakeActorId(machineId, pid, actorIndex)` | 打包成全局 ActorID |
| `MakeProcessId(machineId, pid)` | 打包成 ProcessID（高 24 位补 0） |
| `GetMachineId(id)` / `GetPid(id)` / `GetActorIndex(id)` | 拆解 ActorID 各段 |
| `ActorIdToProcessId(actorId)` | 从 ActorID 提取其所属 ProcessID |
| `IsValidActorId(id)` | ActorIndex 非 0 判合法 |
| `GetOsPid()` | 取 OS 进程号（Windows `GetCurrentProcessId` / POSIX `getpid`） |

```cpp
uint64 actorId = xActorId::MakeActorId(machineId, pid, ++actorIndex);
uint32 mid = xActorId::GetMachineId(actorId);   // 定位物理机
uint32 pid = xActorId::GetPid(actorId);         // 定位进程
uint32 idx = xActorId::GetActorIndex(actorId);  // 定位进程内 Actor
```

## 3. PlayerID → ActorID 路由流程

交互时先有业务 `PlayerID`，路由到其 `PlayerActor` 分三步：

```
目标 PlayerID
   │
   ▼
1. CPlayerActorIndex::Resolve(playerId)
   ├─ 本地缓存 map<playerId, actorId> 命中 → 直接用
   └─ miss → Redis GET player:{playerId} → 回填本地缓存
   │
   ▼
2. 解析 actorId = MakeActorId 布局
   ├─ MachineID + PID → 定位承载进程 (CLinkManager 按 ProcessID 建链/取对端)
   └─ ActorIndex → 定位进程内 Actor (CActorMgr::Find)
   │
   ▼
3. RPC 投递 → 目标进程 CActorMgr::Find(type, actorIndex) → 消息队列 → 处理 → 原路返回
```

- **创建（负载均衡）**：`CreateActorRemote` 用 `PickProcessForActor` 选最低负载进程，负载均衡仅在此生效；创建成功后写 `player:{playerId} → actorId`。
- **失效兜底**：`ActorID` 失效（目标离线/迁移）→ `Invalidate(playerId)` 清缓存 → 回 Redis 重取最新 `ActorID` → 再路由。
- **查无此人**：Redis 也查不到 → 目标不存在（报错或按需新建）。

## 4. 本地缓存与失效策略

| 项 | 策略 |
|---|---|
| 存储 | 进程内 `std::map<int64 playerId, uint64 actorId>`（`CPlayerActorIndex::m_mapPlayerToActor`） |
| 写入 | `Register`（创建成功）/ `Resolve`（Redis miss 回填）时写入 |
| 读取 | `Resolve` 优先缓存命中 |
| 失效 | `Deregister`（卸载）整体清除；`Invalidate`（疑似离线/迁移）只清缓存不回删 Redis |
| 一致性 | 缓存为「加速层」，真值源在 Redis；缓存条目可被随时丢弃，代价是一次 Redis GET |
| 兜底 | 进程崩溃后缓存自然消失；脏缓存靠 `Invalidate` + 目标进程存活校验（心跳）兜底 |

## 5. Redis 键约定

复用服务发现 Redis 池（`cluster_discovery`），与 `cluster:process:*` 同池：

| 键 | 类型 | 值 | 生命周期 |
|---|---|---|---|
| `player:{playerId}` | String | `actorId` 十进制字符串 | 创建 Player Actor 时 `SET`，卸载时 `DEL` |
| `cluster:process:{type}:{id}` | Hash | 进程信息 | 既有（进程注册/心跳） |
| `cluster:process:by_type:{type}` | Set | 进程 ID 集合 | 既有（类型索引） |

> 若 `player:{playerId}` 无 TTL，进程崩溃会留下脏键，需靠「目标进程存活校验 + 失效重查」兜底（见潜在问题）。

## 6. 代码钩子

| 时机 | 位置 | 动作 |
|---|---|---|
| 启动 | `CActorLoadManager::Init` | `CPlayerActorIndex::Init(poolTitle)` |
| 创建成功 | `CActorMgr::Install`（`type==APlayer && id!=0`） | `Register(id, id)` |
| 卸载 | `CActorMgr::UnInstall`（`type==APlayer && id!=0`） | `Deregister(id)` |

> `Install`/`UnInstall` 是本地与远程、gRPC 与 asio RPC 所有创建/卸载路径的唯一汇聚点，在此挂钩可保证
> 「恰好注册/注销一次」，且不侵入 `engine/xRpc/`（RPC 层保持纯转发）。

## 潜在问题

> 本文潜在问题已纳入总登记表 [潜在问题](../潜在问题.md)，由 `tools/gen-problems.js` 从 `problems.json` 自动生成。

<!-- PROBLEMS:START -->
| ID | 状态 | 优先级 | 归期 | 问题 | 处理/方向 |
|---|---|---|---|---|---|
| **C1** | 🔴 仍开放 | 高 | M2 | MachineID 唯一性派生未落地 | 编解码与索引已就位，但 Redis 按物理机唯一 ID 派生 MachineID 的逻辑未实现；若多实例同时注册同机存在竞态，可能拿重复 MachineID 导致跨机 ActorID 碰撞。 |
| **C2** | 🔴 仍开放 | 高 | M2 | PID 复用碰撞 | PID 仅 16 位且 OS 回收复用；同机先后两进程拿到同 PID 时，旧进程残留 player:*/cluster:actor:* 索引指向错误进程；需进程心跳存活校验 + 启动清理本 PID 残留。复核：16 位 PID 仅用于全局 ActorID 打包，路由 processId 走 Redis INCR 序列、OS PID 以 int64 全宽存 os_pid，故本项只影响全局 ActorID 段、不影响进程寻址。 |
| **C3** | 🔴 仍开放 | 中 | M2 | ActorIndex 24 位溢出回绕 | 约 1677 万上限；不重启累计创建超限时自增回绕会撞现存实例；分配时需检测回绕并拒绝或复用已释放索引。 |
| **C4** | 🔴 仍开放 | 中 | M2 | PID 位宽不足（OS PID 可超 65535） | Linux 默认 PID 上限 4M，打包进 16 位会截断、低位相同而碰撞；需限制 PID 上限或改用 24 位 PID 段（会挤占 ActorIndex/保留位）。复核：16 位 PID 仅用于全局 ActorID 打包的进程段、不参与路由；路由 processId 走 Redis INCR、OS PID 全宽存 os_pid、local.info 存稳定 processId，故 OS PID 超 65535 不影响寻址，设计无问题。 |
| **C5** | 🔴 仍开放 | 高 | M2 | 跨进程缓存一致性 | 本地缓存为纯内存快照、无主动失效推送；A 改 B 缓时 B 读到过期 ActorID，需依赖 Invalidate+失败回查，或改 Redis Pub/Sub 广播失效。 |
| **C6** | 🔴 仍开放 | 高 | M2 | 进程崩溃脏缓存/脏键（player:* 无 TTL） | 崩溃不清 Redis 残留 player:{id} 脏键；他进程缓存可能仍指向死进程；兜底=目标进程存活校验+路由失败回查+player:* 加 TTL 定期清理。 |
| **C7** | 🔴 仍开放 | 中 | M2 | 过渡态 id→id 恒等映射 | ActorID 尚未切换全局自描述（实例 ID 沿用业务 roleId），Register(id,id) 暂存恒等映射；全局 ID 接入创建链路前，本索引无跨进程定位实际收益。 |
<!-- PROBLEMS:END -->
