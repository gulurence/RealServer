# 集群动态扩缩容 + WebPlatform 配置管理 — 设计方案

> 状态：方案草案（待确认关键决策后实施）
> 关联：`docs/multiplayer-design.md`、`xActorId.h`（MachineID 编码）、`xCluster/`（进程/发现/负载）

## 1. 需求 → 方案映射

| # | 需求 | 方案组件 | 状态 |
|---|---|---|---|
| 1 | 物理机 → 进程 → Actor 负载层次 | NodeManager 机器注册 + 进程 machine_id + Actor loadScore | 部分已有（MachineID 已编码进 ID，缺运行时注册） |
| 2 | 进程按 CPU/内存自动上限管理，后台可设 | 心跳扩展上报 cpu/mem + 阈值配置 + 负载均衡感知资源 | 新增 |
| 3 | WebPlatform 动态控进程数量 + 配置 Redis 化 + Lua 热更(命令通道) | 运行时配置 store + 集群控制页 + GM `reload` pub/sub 通道 | 新增 |
| 4 | 进程宕机动态拉起 | NodeManager 子进程守护（崩溃检测 + 退避重启） | 新增 |
| 5 | 进程总量上限，依物理机容量计算 | NodeManager 容量上报 + 总量上限执行 | 新增 |
| 6 | 单独集群控制页面 | WebPlatform `cluster.php` | 新增 |
| 7 | 配置专门分类页 | WebPlatform `settings*.php` | 新增 |

## 2. 现状盘点

**已有（可直接复用/扩展）：**

- **进程模型**：单 exe 多进程，`EProcessType` = Gate(100)/Login(101)/Global(102)/Game(201)。
  `CProcessManager` 负责 CreateProcess（Redis 自增 ID + 端口分配）→ RegisterInDiscovery → StartHeartbeat(5s)。
- **服务发现**：`CServiceDiscovery` 写 `cluster:process:{type}:{id}` Hash
  （含 `load`=actor 数、`state`、`heartbeat`、`ip`、`authkey`），`cluster:process:by_type:{type}` Set。
- **Actor 负载均衡**：`CActorLoadManager::PickProcessForActor` → `CLinkManager::GetBestPeer`
  （当前策略 = 挑 loadScore 最低的 Running 对端）。
- **控制面**：`CGmCommandMgr` Redis pub/sub，频道 `cluster:gm:all/type:{t}/pid:{p}`，
  框架命令 help/info/processes/actors/spawn/shutdown/loglevel，Actor 命令 `actor <t> <id> <cmd>`，
  应答写 `cluster:gm:replies` 列表。
- **MachineID 编码**：`xActorId.h` 已把 MachineID(24bit) 编进 ActorID/ProcessID，路由自描述，
  **物理机概念在 ID 层已是一等公民**，缺的是运行时的机器注册与分配。
- **WebPlatform**：PHP + RedisCluster，已有 nodes/accounts/actors/trace/gm 等只读页。

**缺失（需新增）：**

1. 机器注册表（`cluster:node:*`）—— 无 machineId 分配、无容量信息。
2. 进程守护/调度器（NodeManager）—— 目前进程全靠外部 driver.sh 手动起，无拉起/扩缩容。
3. CPU/内存指标 —— `loadScore` 只是 actor 数，无真实资源采样。
4. 资源感知负载均衡 —— `GetBestPeer` 只看 actor 数，不看 cpu/mem 阈值。
5. 运行时配置 store —— 配置写死在 `server.xml`，WebPlatform 改不了。
6. Lua 热更 —— `LuaVM` 有 DoFile/DoString/LoadAllActors，但无运行中重载命令。
7. 集群控制/配置页 —— WebPlatform 只有只读监控。

## 3. 总体架构

```
                        ┌──────────────────────────────┐
                        │      WebPlatform (PHP)         │
                        │  监控页 / cluster 控制页 / 配置页 │
                        └──────────────┬───────────────┘
                              写配置/期望数    读状态    发命令(pub/sub)
                                       │            │
                    ┌──────────────────▼────────────▼──────────────┐
                    │            Redis Cluster (7000-7005)          │
                    │  集群状态 + 运行时配置 + 期望状态 + GM 控制面     │
                    └───┬───────────────┬────────────────┬──────────┘
                        │读期望/写状态   │读阈值/写心跳     │订阅命令
        ┌───────────────▼──────┐  ┌────▼─────────────────▼──────┐
        │  NodeManager(机器守护) │  │   RealServer 进程(Gate/Login/…)  │
        │  每物理机一个          │  │   心跳+指标 / 负载感知 / GM/热更   │
        │  •机器注册/容量上报     │  │                              │
        │  •起停子进程/崩溃拉起   │  └──────────────────────────────┘
        │  •指标采集             │
        │  •本地容量上限执行      │
        └───────────────────────┘
```

**分层职责（对齐需求 1 的"物理机 → 进程 → Actor"）：**

- **物理机层**：`NodeManager`（**独立轻量二进制**，单独 CMake target，不链接 Lua/Actor/RPC）。
  每台物理机一个。负责机器注册、容量上报、子进程起停/守护、指标采集、本地进程总量上限。
- **进程层**：现有 RealServer 进程，扩展心跳上报 cpu/mem/machine_id，负载均衡感知阈值。
- **Actor 层**：`CActorLoadManager`/`CLinkManager` 扩展为"资源感知"挑进程（actor 数 + cpu/mem 加权 + 上限过滤）。

## 4. 核心组件设计

### 4.1 NodeManager（机器守护进程）

**独立轻量二进制 `NodeManager`**：单独 CMake target，只链接 `xBase`（日志/sysinfo/配置）+ `xRedis`
（`CRedisCli` RedisCluster），**不链接** Lua/Actor/SWIG/asio-RPC。与 RealServer.exe 同目录产出，负责拉起后者。

启动参数：`NodeManager --machine-id=<id> --platform=dev --config=config/server.xml`（读 server.xml 拿 Redis seed + auth secret）。

职责：

1. **机器注册**：用 `--machine-id` 作为 machineId（**单机 Docker 模拟多机器时由部署方显式指定**，
   每容器/每实例一个唯一 id；真多机时可用 hostname 哈希 + IP 校验去重）。
   写 `cluster:node:{machineId}` Hash = `{machine_id, hostname, ip, cpu_cores, mem_total_mb, os, state, heartbeat}`，
   加入 `cluster:node:all` Set。**machineId 本机落地文件缓存**，重启复用避免 ID 漂移。
2. **子进程生命周期**：读期望状态（见 4.4）→ 按需 `CreateProcess` 启动/停止
   `RealServer.exe --type=X --name={type}-{id} --machine={machineId} --platform=...`。
   **端口分配**：`GetBasePortForType` + `machineId * 端口块偏移`，保证多 NodeManager（模拟多机器）同宿主机时不冲突。
3. **崩溃拉起**：为每个子进程持 handle + 状态表 `{type,id,name,restart_count,last_start}`。
   监视线程检测退出；**非优雅退出**（未走 `shutdown` 命令）则退避重启（1s→2s→4s…封顶 60s），
   `restart_count` 超阈值（配置）则标记 faulted + 写告警 `cluster:alert`，停止无限重启。
4. **指标采集**：周期性采样本机 cpu/mem + 各子进程 cpu/RSS，写回 Redis（见 4.2）。
5. **容量上限**：维护"本机允许的进程总量"（依 cpu_cores/内存 + 每类型权重计算，可被 WebPlatform 覆盖），
   超限拒绝再起新进程。

> **跨平台**：Windows 用 `GetSystemTimes`/`GlobalMemoryStatusEx` + `GetProcessTimes`/`GetProcessMemoryInfo`；
> Linux 用 `/proc/stat`、`/proc/meminfo`、`/proc/<pid>/stat`。封装成 `xBase/xSysInfo.h` 工具层，双平台实现。

### 4.2 进程指标与心跳扩展

`ProcessInfo`（`xProcessDefine.h`）新增字段：

```cpp
uint32      machineId   = 0;    // 所属物理机
int32       cpuPercent  = 0;    // 自进程 CPU 占比 (0-10000, 万分数或整数百分比)
int64       memRssMb    = 0;    // 自进程 RSS (MB)
int32       memPercent  = 0;    // 占本机总内存百分比
```

- `CProcessManager::HeartbeatLoop`（现 5s）在每次心跳时**顺带采样**自身 cpu/mem，随心跳一起 `HSET` 进
  `cluster:process:{type}:{id}`。CPU% 用 `GetProcessTimes` 两次差值计算；mem 用 `GetProcessMemoryInfo`。
- `machineId` 由 `main.cpp` 从 `--machine=` 参数（NodeManager 传入）解析进 `ProcessXmlConfig`，
  落到 `ProcessInfo`，注册/心跳时写进 Hash。**本机单进程开发（无 NodeManager）时 machineId=0 走兼容路径。**

### 4.3 资源感知负载均衡

`CActorLoadManager::PickProcessForActor` / `CLinkManager::GetBestPeer` 从"最低 loadScore"升级为：

1. **硬过滤**：仅保留 `state==Running` 且满足 `actor_count < actor_max`、`cpu < cpu_full`、`mem < mem_full` 的对端。
2. **加权打分**：`score = w1*actor_count + w2*cpu + w3*mem`（权重可配，默认 actor 优先），选最低分。

阈值 `actor_max/cpu_full/mem_full` 来自 Redis 运行时配置（见 4.5），由各进程周期性拉到本地缓存，
避免每次分配都查 Redis（配 5-10s 刷新 + 订阅变更）。

### 4.4 运行时配置存储（Redis 化，需求 3/7）

`server.xml` 降级为**仅引导信息**（Redis seed 连接、进程 type/name/platform、auth secret）。
所有**运行期可调**配置迁到 Redis，由 WebPlatform 写、各组件读：

```
cluster:config:cluster            Hash   {platform, log_level, auto_scale_enabled, controller_interval}
cluster:config:process:{type}     Hash   {min_count, max_count, cpu_warn, cpu_full, mem_warn_mb,
                                          mem_full_mb, actor_max, weight}
cluster:config:node:{machineId}   Hash   {enable, capacity_cap, role}
cluster:desired:{type}            Hash   {count, mode}    # mode = manual|auto, 期望进程总数
cluster:desired:{type}:{machineId} String  count           # 每机器期望数(可选, 精确放置)
```

- **读取方**：NodeManager（期望数/容量上限）、进程（阈值，写进负载均衡缓存）、WebPlatform（展示+回填）。
- **一致性**：配置变更走 pub/sub 通知（`cluster:config:notify`），各进程订阅后主动重拉，避免轮询延迟。

### 4.5 Lua 热更命令通道（pub/sub，脚本层实现后续定）

**本期只做"命令通道"，不做脚本层热更**：让 WebPlatform 能通过 Redis pub/sub 把 `reload` 命令送到进程并被框架
分发器收到/应答；具体怎么重载 Lua 脚本（重跑 init.lua / on_reload 回调 / 重绑 delegate）**后续再定**。

复用 `CGmCommandMgr` 既有 pub/sub 控制面（`cluster:gm:all / type:{t} / pid:{p}`），在框架命令表里注册：

```
reload [<actorType> [<id>]]   # 占位命令：收到即应答，脚本层热更逻辑 TODO
```

执行路径：WebPlatform `PUBLISH cluster:gm:*` → 进程 `SUBSCRIBE` 收到 → `CGmCommandMgr::HandleCommand`
→ 框架命令分发到 `reload` handler（**stub**：写一条 GM 应答"reload 已收到，脚本层热更未实现"，暂不触碰 LuaVM）。

说明：
- 通道本身（PUBLISH/SUBSCRIBE）在 Redis Cluster 下全集群广播，无需改造即可送达；
  应答列表 `cluster:gm:replies` 是 key 命令，需 `CGmCommandMgr` 迁到 RedisCluster 才能正确 RPUSH（见 §7 决策 5，P2 前置）。
- "直接发送进程命令"的 RPC 通道（`RPC_METHOD_RELOAD_LUA`）与脚本层热更实现一并后置。

### 4.6 自动扩缩容（需求 2/5）

两档（**已确认：手动优先，自动二期**）：

1. **手动（首期交付）**：WebPlatform 写 `cluster:desired:{type}.count` → NodeManager 收敛到期望数。这是首期基线能力。
2. **自动（二期）**：NodeManager 间用 Redis `SETNX cluster:lock:controller` + TTL 选出一个 Leader，
   Leader 周期性评估：
   - **扩容**：某类型所有进程平均 cpu > `cpu_warn` 且均接近 `cpu_full`，且 `count < max_count` → +1，
     按 `GetHostedActorTypes` 权重 + 机器剩余容量选目标机。
   - **缩容**：平均 cpu < 下限（如 30%）且 `count > min_count` → 先对该进程 `spawn 卸载`/停止新分配、排空 Actor，再 −1。
   - **容量校验（需求 5）**：任何扩缩都满足 `Σ count(type)*weight(type) ≤ machine.capacity_cap`，
     `capacity_cap` 由 `cpu_cores`/内存按权重表折算，WebPlatform 可覆盖。

## 5. Redis 数据模型汇总

```
# 机器
cluster:node:{machineId}        Hash  {machine_id, hostname, ip, cpu_cores, mem_total_mb, state, heartbeat}
cluster:node:all                Set   {machineId...}
cluster:node:id_seq             String (自增分配 machineId)

# 进程（扩展 4.2 字段）
cluster:process:{type}:{id}     Hash  {..., machine_id, cpu_percent, mem_rss_mb, mem_percent}
cluster:process:by_type:{type}  Set

# 运行时配置（4.4）
cluster:config:cluster          Hash
cluster:config:process:{type}   Hash
cluster:config:node:{machineId} Hash
cluster:config:notify           pub/sub 频道 (配置变更通知)

# 期望状态
cluster:desired:{type}          Hash {count, mode}
cluster:desired:{type}:{machineId} String

# 控制面（已有）
cluster:gm:all / :type:{t} / :pid:{p}   pub/sub
cluster:gm:replies                      list
cluster:lock:controller                 SETNX + TTL (Leader 选举)

# 告警
cluster:alert                     list (NodeManager 崩溃/超限/扩缩事件)
```

## 6. WebPlatform 页面设计

| 页面 | 类型 | 内容 |
|---|---|---|
| `cluster.php` | **新增·集群控制** | 机器列表（cpu/内存条）、各类型进程数（在线/离线/CPU/内存/Actor 数）、手动扩缩容按钮（+/-/设数量）、扩缩容事件流 |
| `settings_cluster.php` | 新增·配置分类 | 全局：platform、log_level、auto_scale 开关、controller 间隔 |
| `settings_scaling.php` | 新增·配置分类 | 每类型：min/max_count、cpu_warn/full、mem_warn/full、actor_max、weight |
| `settings_nodes.php` | 新增·配置分类 | 每机器：enable、capacity_cap、role |
| `reload.php` | 新增·热更(通道) | 选 actor 类型/全部 → 发 `reload` GM 命令（仅占位应答，脚本层热更后续定） |
| `nodes.php` | 改造 | 加 machine 分组 + cpu/mem 列 |
| `config.php` | 改造 | 加 `scan_cluster` 复用、配置读写 helper |

统一在顶部导航加「集群控制」「配置」分类；复用现有 `style.css`。

## 7. 关键决策（已确认）

1. **NodeManager 形态** = **独立轻量二进制** `NodeManager.exe`（单独 CMake target，不链接 Lua/Actor/RPC）。
2. **自动扩缩容节奏** = **手动优先，自动二期**（首期：期望数 + 崩溃拉起 + 容量上限）。
3. **Lua 热更** = **只做 pub/sub 命令通道**（`reload` 框架命令 + 收发），脚本层具体怎么热更后续再定。
4. **部署形态** = **单机多进程（Docker 模拟多机器）**，machineId 由部署方显式指定。
5. **GM 控制面集群化**（必做前置）：`CGmCommandMgr` 现仍走 hiredis 单机（Redis→Cluster 迁移"GM 后置"遗留），
   热更/控制命令要落集群，先迁到 redis++ `RedisCluster`（或复用 `CRedisCli`）。

## 8. 实施路线（分阶段）

| 阶段 | 内容 | 产出 |
|---|---|---|
| P0 | 决策已确认（§7），冻结方案 | 本设计文档 |
| P1 | 进程侧指标+心跳扩展（4.2）+ 资源感知负载均衡（4.3） | 进程上报 cpu/mem，LB 按阈值过滤 |
| P2 | GM 控制面集群化（决策5）+ `xSysInfo` 采样工具 | GM/热更落 RedisCluster |
| P3 | NodeManager 独立二进制（4.1）：机器注册/起停/崩溃拉起/容量 | 守护 + 手动扩缩容 |
| P4 | 运行时配置 store（4.4）+ WebPlatform 配置分类页 + cluster 控制页（§6） | WebPlatform 可控集群 |
| P5 | reload 命令 pub/sub 通道（4.5，stub）+ reload 页面 | GM `reload` 命令收发（脚本层热更后置） |
| P6 | 自动扩缩容（4.6，二期）+ 端到端联调 | Leader 控制器 + 压测验证 |

## 9. 风险

- **端口分配**：同机多实例需严格按 base-port 区间分配，且要检测端口占用（已有 `YunDetectService` 抢 10000 的历史坑）。
- **心跳/指标频率**：5s 心跳 + 指标采样，集群规模大时 Redis 写放大；可考虑指标独立 10-15s 周期。
- **Leader 脑裂**：Redis SETNX+TTL 需续租 + 乐观锁，避免双 Leader 并发扩缩。
- **缩容排空**：停止进程前必须先把其 Actor 迁移/排空，否则丢会话（玩家 Actor 需"先迁后杀"或标记只读）。
- **MachineID 漂移**：机器 ID 必须本机持久化 + IP 校验，防止重启后 ID 变化导致 ActorID 路由失效。
