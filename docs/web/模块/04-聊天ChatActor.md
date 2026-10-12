# 聊天模块（ChatActor）

> 里程碑 M4 · Actor 类型 `EActorType_AChat=104`（Global 进程）

## 定位

聊天单例 Actor（`id=0`），按频道缓存并分发消息。频道以 `"channelType:channelId"` 标识。

## 消息 ID（`chat.proto`）

| 常量 | 值 |
|------|----|
| `SEND_REQ` / `SEND_RSP` | 1 / 2 |
| `NTF` | 3 |
| `HISTORY_REQ` / `HISTORY_RSP` | 4 / 5 |

## 频道类型

| 常量 | 值 |
|------|----|
| `NONE` | 0 |
| `ROOM` | 1 |
| `BATTLE` | 2 |

## 状态

- `history`：`"channelType:channelId" → { {senderId, senderName, content, timestamp}, ... }`
- `MAX_HISTORY = 50`（环形淘汰）

## 关键流程

- `handle_send`：写入频道历史（超 50 条淘汰最旧）→ 回 `SendRsp`。
- `handle_history`：拼 `"senderId,senderName,content,timestamp;..."` 字符串。

## 潜在问题

> 本文潜在问题已纳入总登记表 [潜在问题](../潜在问题.md)，由 `tools/gen-problems.js` 从 `problems.json` 自动生成。

<!-- PROBLEMS:START -->
| ID | 状态 | 优先级 | 归期 | 问题 | 处理/方向 |
|---|---|---|---|---|---|
<!-- PROBLEMS:END -->
