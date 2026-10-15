# 观战模块（SpectatorActor）

> 里程碑 M6 · Actor 类型 `EActorType_ASpectator=204`（Battle 进程，多实例）

## 定位

观战玩家只读帧流、不参与模拟。同一战斗房的所有观战者共享一个 SpectatorActor 实例（`id == battle_id`），进入时快照对齐，之后追帧播放。

## 消息 ID（`spectator.proto`）

| 常量 | 值 |
|------|----|
| `ENTER_REQ` / `ENTER_RSP` | 1 / 2 |
| `LEAVE_REQ` / `LEAVE_RSP` | 3 / 4 |
| `SNAPSHOT_NTF` | 5 |
| `FRAME_NTF` | 6 |
| `END_NTF` | 7 |

## per-instance 状态

- `spectators`：`pid → true`（观战席位）

## 关键流程

- `handle_enter`：登记观战席位 → 回 `EnterRsp(battleId)`。
- `handle_leave`：注销席位 → 回 `LeaveRsp`。

## 潜在问题

> 本文潜在问题已纳入总登记表 [潜在问题](../潜在问题.md)，由 `tools/gen-problems.js` 从 `problems.json` 自动生成。

<!-- PROBLEMS:START -->
| ID | 状态 | 优先级 | 归期 | 问题 | 处理/方向 |
|---|---|---|---|---|---|
<!-- PROBLEMS:END -->
