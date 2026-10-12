# 战斗模块（BattleActor）

> 里程碑 M5 · Actor 类型 `EActorType_ABattle=203`（Game 进程，多实例）

## 定位

帧同步战斗房，**多实例** Actor（`id == battle_id`）。由 Room 开局时经 `balanceCreate(AT.BATTLE, battleId)` 跨进程创建；`CLuaActor::Create` 为每个实例生成独立 Lua 表（`ctor` 初始化 per-instance 状态）。

确定性战斗核心为纯 C++ 模块 `shared/battlecore`，通过 `si_battlecore_*` SWIG 绑定暴露给 Lua；种子由 `battle_id` 派生，保证客户端/服务器一致。

## 消息 ID（`battle.proto`）

| 常量 | 值 |
|------|----|
| `ENTER_REQ` / `ENTER_RSP` | 1 / 2 |
| `START_NTF` | 3 |
| `INPUT_REQ` | 4 |
| `FRAME_NTF` | 5 |
| `SNAPSHOT_NTF` | 6 |
| `END_NTF` | 7 |
| `LEAVE_REQ` / `LEAVE_RSP` | 8 / 9 |
| `HASH_NTF` | 10 |

## 错误码

| 常量 | 值 |
|------|----|
| `BATTLE_AUTH_FAILED` | 2301 |

## per-instance 状态

- `players`（`pid → {name, spawnIndex}`）、`entered`（`pid → true`）
- `core`（battlecore 句柄）、`seed`、`frame`、`finished`

## 关键流程

1. **进入** `handle_enter`：校验 Redis `battle:auth:{battleId}` → `ensure_core` → 回 `EnterRsp(seed, frame, players)`。
2. **核心初始化** `ensure_core`：`seed = (battleId % 0xFFFFFFFF) + 1` → 读 Redis `battle:roster:{battleId}` → `si_battlecore_new/init`。
3. **逐帧输入** `handle_input`：解嵌套 `inputMsg` 的 moveDir/shootDir/skillId/fire → `submit_input` → `frame += 1` → `tick` → `check_win`。
4. **一致性哈希** `handle_hash`：比对客户端/服务器 `si_battlecore_hash`，不一致告警。
5. **退出** `handle_leave`：全员离开后 `si_battlecore_free`。

## 潜在问题

> 本文潜在问题已纳入总登记表 [潜在问题](../潜在问题.md)，由 `tools/gen-problems.js` 从 `problems.json` 自动生成。

<!-- PROBLEMS:START -->
| ID | 状态 | 优先级 | 归期 | 问题 | 处理/方向 |
|---|---|---|---|---|---|
<!-- PROBLEMS:END -->
