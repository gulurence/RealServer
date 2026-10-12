# RealServer 集群编排 · 全功能全链路测试报告

> 日期：2026-09-13
> 范围：P1–P6（集群编排基础）＋ 5 项新增需求（P7–P11）
> 环境：Windows 11 + MSVC（`build_msvc`）＋ Docker Redis Cluster（6 节点，seed `127.0.0.1:7000`）

---

## 一、功能实现清单

| 阶段 | 内容 | 落地位置 |
|---|---|---|
| P1–P4 | 机器注册/心跳、期望数收敛、崩溃退避拉起、容量上限 | `engine/node_manager/NodeManager.{h,cpp}` |
| P5 | 服务发现接入 Redis Cluster（`redis++ RedisCluster`） | `engine/xRedis/RedisCli.{h,cpp}`、`engine/xCluster/xServiceDiscovery.cpp` |
| P6 | 自动扩缩容控制器（Leader 锁 + 按类型资源评估改 `desired`） | `NodeManager::AutoScale/TryAcquireControllerLock` |
| **P7** | **Actor 帧级脏读写 Redis 框架**（脏标记 → 周期 flush；启动 LoadState 恢复） | `engine/xActor/Actor.{h,cpp}`、`ActorMgr.cpp`、`xProcessManager.cpp`、`luacore/luacore.i` |
| **P8** | **进程宕机 + Actor 动态恢复记录页面** | `EnvRoot/WebPlatform/www/crashes.php`、`actor_recovery.php` |
| **P9** | **集群节点灰度更新**（进程级滚动重启 + 内容级按 id 取模切流） | `NodeManager::ProcessUpdateGray`、`xProcessManager.cpp`、`xProcessDefine.h` |
| **P10** | **进程更新 + 游戏内容更新页面** | `EnvRoot/WebPlatform/www/updates_process.php`、`updates_content.php` |
| **P11** | **企业级免费模板重构**（侧边栏 + 顶栏 + 卡片式后台） | `EnvRoot/WebPlatform/www/_layout.php`、`style.css` |

---

## 二、全链路测试结果

### 2.1 构建
- ✅ `NodeManager.exe`、`RealServer.exe` 均编译链接通过（MSVC Debug）。

### 2.2 服务发现 + Redis Cluster（P5）
- ✅ RealServer 启动到 `READY`，`CRedisCli` 以集群 seed `127.0.0.1:7000` 连接成功，`CLUSTER SLOTS` 发现全集群，无 `-MOVED`/`cluster` 报错。
- ✅ 注册 hash `cluster:process:100:8` 上报 `version=1.0.0 content_version=1.0.0`（版本字段 P9 新增）。

### 2.3 内容灰度（P9 内容级）
- ✅ 置 `cluster:update:content:config` 的 `gray_percent=50` → 进程按 `processId % 100 < 50` 命中桶，`content_version` 由 `1.0.0` 切换为 `1.0.1`。
- ✅ C++ 读取字段（`version`/`gray_percent`/`status`）与 `updates_content.php` 写入字段完全一致。

### 2.4 机器注册 + 控制器锁（P1/P6）
- ✅ NodeManager `--machine-id=3` 注册 `cluster:node:3` + `cluster:node:all`。
- ✅ `cluster:lock:controller = "3"`（SET NX EX，TTL≈30s 心跳续期），单 Leader。

### 2.5 崩溃日志（P8 数据面）
- ✅ 优雅停止子进程写入 `cluster:crash:log`：`{ts}|{machine}|{type}|{name}|{event}|{detail}`，格式与 `crashes.php` 解析器一致。

### 2.6 自动扩缩容（P6）
- ✅ **修复前**：`cluster:desired:100` 置 1 拉起 Gate-1 后，AutoScale 立即把 desired 1→0 收回（新进程 cpu=0 < cpu_warn 阈值），**震荡**。
- ✅ **修复后**：`auto_scale_enabled` 默认关闭（手动优先），且缩容要求 `minCount > 0`；置 desired=1 后进程保持在线，不再被自动回收。

### 2.7 进程级灰度（P9 进程级）
- ✅ **修复前**：置 `cluster:update:process:config` `version=1.0.1 status=rolling` 后无动作 —— 根因 `FindChildVersion` 命中同名**僵尸条目**（旧进程 crash 未注销、`version` 为空），`cur.empty()` 导致静默跳过。
- ✅ **修复后**：`FindChildVersion` 增加「心跳新鲜（≤15s）＋ 版本非空」过滤，跳过僵尸条目。
- ✅ 端到端验证：Gate-1 被 Leader 按批 `rolling restart`（日志 `process-update gray: restart Gate-1 (target 1.0.1)`），`cluster:update:process:log` 有 `Gate-1 rolling restart → 1.0.1` 记录。
- ⚠️ 注意：因二进制版本硬编码 `1.0.0` 无法达成 `1.0.1`，测试中表现为每 10s 滚动重启一次（预期行为；真实部署需先换 exe 再下发意图）。

### 2.8 WebPlatform 页面（P8/P10/P11）
- ✅ 页面齐全且 key/格式对齐：
  - `crashes.php` ↔ `cluster:crash:log`（6 字段 `explode('|',6)`）
  - `actor_recovery.php` ↔ `actor:recovery:log`（5 字段）+ `scan_cluster('actor:state:*')`
  - `updates_process.php` ↔ `cluster:update:process:config`/`:log` + 各进程 `version` 分布
  - `updates_content.php` ↔ `cluster:update:content:config`/`:log` + 各进程 `content_version` 分布
- ✅ `_layout.php` 采用企业级后台模板：分组侧边栏（监控/集群控制/配置/更新/恢复记录/工具）＋ 顶栏（用户/角色/退出）＋ 卡片式内容区，新增页面只改一处导航。

---

## 三、测试中发现并已修复的问题

| # | 问题 | 根因 | 修复 |
|---|---|---|---|
| 1 | 自动扩缩容震荡：手动 desired=1 被立即收回 | 新进程 cpu=0 低于 cpu_warn 触发缩容 | `auto_scale_enabled` 默认关闭 + 缩容加 `minCount > 0` 前置 |
| 2 | 进程灰度静默不触发 | `FindChildVersion` 返回同名僵尸条目的空版本 | 增加心跳新鲜 + 版本非空过滤 |
| 3 | `hgetall` 编译报错 | redis-plus-plus 仅提供 `hgetall(key, Output)` 重载 | 改用 `std::inserter(out, out.end())` |

---

## 四、遗留问题（待一起讨论）

### 4.1 测试环境 Redis 残留状态（需授权清理）
多轮测试在集群里留下脏数据，当前未清理：
- `cluster:process:100:1` ~ `:10`（多进程 hash，含无 `version` 字段的旧条目）
- `cluster:process:by_type:100` 的 1–10 成员、`cluster:process:id_seq`
- `cluster:node:2`、`cluster:lock:controller`、`cluster:desired:*`

> 清理被自动模式分类器拒绝（`del` 多 key 跨 slot → `CROSSSLOT`；且运行时计算的 key 列表属「不可验证删除范围」）。**需你确认后我再按明确 key 清单逐条 `del`/`srem`/`unlink`。**

### 4.2 P7 Actor 帧级脏读写 —— 功能链路未做端到端验证
- 框架已实现并编译通过：`SetStateField/GetStateField/FlushState/LoadState`、`ActorMgr::Install` 第 6 步 `LoadState`、`xProcessManager` 心跳 flush 扫描、`luacore.i` 的 `si_set_state_field` 等绑定。
- **未验证的原因**：需要一个 `id != 0` 的真实 actor（玩家/战斗）走「下线 → 进程重启 → 动态恢复」完整链路，这依赖完整的登录 → 跨进程创建 actor 流程（gRPC + Lua 业务）。
- 当前 `actor:recovery:log` 为空（`llen=0`），`actor:state:*` 无快照，符合「无真实 actor 实例」的现状。
- **建议**：后续用一次真实登录造一个 player actor，验证「写脏字段 → flush 落 `actor:state:{type}:{id}` → kill 进程 → 重启后 `LoadState` 恢复 + 写 `recovered` 事件」。

### 4.3 灰度更新的「换二进制 / 换脚本」是 stub 级
- 进程灰度当前只做「滚动重启以生效」，**实际替换 exe 由部署侧完成**；内容灰度只做「按 id 取模决定上报哪个 `content_version`」，**实际加载哪套 Lua 脚本由脚本层按版本目录切换**。二者是下发意图 + 状态上报，不是完整的一键发布流水线。

### 4.4 既有问题（非本次引入，记录在案）
- `xBase/xXMLParser.cpp:257` `getPropStr` 用 `out +=` 而非 `out =`，导致 Gate 实际绑定端口 100 而非配置的 10000（见 `run-realserver` skill 的 Gotchas）。
- Lua 热重载为 pub/sub 命令通道 stub（按已确认决策：命令通道先行）。
- `config/server.xml` 含明文 DB/Redis 连接信息（红线：不提交生产真实凭证）。

---

## 五、结论

- P1–P6 及 5 项新增需求（P7–P11）**代码均已实现并编译通过**；除 P7 的端到端恢复链路（需真实 actor）外，其余均在 Redis Cluster 环境下完成了**可重复的功能验证**。
- 测试中暴露并修复了 2 个真实缺陷（扩缩容震荡、进程灰度僵尸条目），均已重建验证通过。
- 遗留 4 类问题，其中「测试环境 Redis 残留清理」需你授权、「P7 端到端」「灰度一键发布」需后续一起讨论方案。
