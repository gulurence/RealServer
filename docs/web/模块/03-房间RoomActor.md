# 房间模块（RoomActor）

> 里程碑 M3 · Actor 类型 `EActorType_ARoom=103`（Game 进程）

## 定位

房间管理单例 Actor（`id=0`），负责建房/列表/加入/离开/准备/踢人/转让/开局，开局时生成 `battle_id` + `auth_key` 并跨进程创建 BattleActor。

## 消息 ID（`room.proto`）

| 常量 | 值 |
|------|----|
| `CREATE_REQ` / `CREATE_RSP` | 1 / 2 |
| `LIST_REQ` / `LIST_RSP` | 3 / 4 |
| `JOIN_REQ` / `JOIN_RSP` | 5 / 6 |
| `LEAVE_REQ` / `LEAVE_RSP` | 7 / 8 |
| `READY_REQ` / `READY_RSP` | 9 / 10 |
| `KICK_REQ` / `KICK_RSP` | 11 / 12 |
| `TRANSFER_REQ` | 13 |
| `START_REQ` / `START_RSP` | 14 / 15 |
| `INFO_NTF` / `MEMBER_NTF` | 16 / 17 |

## 模式与可见性

- `RoomMode`：`NONE=0` `PVE=1` `PVP=2`
- `RoomVisibility`：`NONE=0` `PUBLIC=1` `PRIVATE=2` `PASSWORD=3`

## 错误码

| 常量 | 值 |
|------|----|
| `ROOM_NOT_FOUND` | 2101 |
| `ROOM_FULL` | 2102 |
| `ROOM_WRONG_PASSWORD` | 2103 |
| `ROOM_NOT_OWNER` | 2104 |
| `ROOM_NOT_ALL_READY` | 2105 |
| `ROOM_ALREADY_IN` | 2106 |

## 状态

- `rooms`：`roomId → { ownerId, mode, visibility, password, maxMembers, state, members }`
- `playerRoom`：`pid → roomId`（反查）
- `nextRoomId`（自 1000）/ `nextBattleId`（自 5000）自增

## 关键流程

- `handle_create`：已在房先退 → 建房（房主即成员）。
- `handle_join`：密码/满员/重复校验后入房。
- `handle_start`：校验房主 + 全员就绪 → 生成 `battle_id`/`auth_key` → 写 Redis `battle:roster:{id}`（逗号分隔 pid）与 `battle:auth:{id}` → `balanceCreate(AT.BATTLE, battleId)` → 回 `StartRsp`。

## 潜在问题

> 本文潜在问题已纳入总登记表 [潜在问题](../潜在问题.md)，由 `tools/gen-problems.js` 从 `problems.json` 自动生成。

<!-- PROBLEMS:START -->
| ID | 状态 | 优先级 | 归期 | 问题 | 处理/方向 |
|---|---|---|---|---|---|
<!-- PROBLEMS:END -->
