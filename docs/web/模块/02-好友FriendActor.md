# 好友模块（FriendActor）

> 里程碑 M2 · Actor 类型 `EActorType_AFriend=102`（Global 进程）

## 定位

好友关系中心，单例 Actor（`id=0`）。客户端好友请求经 Gate 注入发送方 `player_id` 后，由 `RPC_METHOD_FORWARD_ACTOR` 转发到 `on_rpc_forward`。

## 消息 ID（`friend.proto`）

| 常量 | 值 | 说明 |
|------|----|------|
| `ADD_REQ` / `ADD_RSP` | 1 / 2 | 加好友申请 / 回包 |
| `ADD_APPLY_NTF` | 3 | 申请通知（推送给目标） |
| `ADD_ACCEPT_REQ` / `ADD_ACCEPT_RSP` | 4 / 5 | 同意/拒绝申请 |
| `LIST_REQ` / `LIST_RSP` | 6 / 7 | 好友列表 |
| `DEL_REQ` / `DEL_RSP` | 8 / 9 | 删除好友 |
| `ONLINE_NTF` | 10 | 上线通知 |
| `INVITE_REQ` / `INVITE_NTF` | 11 / 12 | 组队邀请 |

## 错误码

| 常量 | 值 |
|------|----|
| `ALREADY_FRIEND` | 2001 |
| `NOT_FRIEND` | 2002 |

## 状态

- `friends`：`playerId → { [friendId] = true }`（双向关系）
- `pending`：`targetId → { [applicantId] = true }`（待处理申请）
- `names`：昵称尽力缓存

## 关键流程

- `handle_add`：已互为好友回 `ALREADY_FRIEND`；否则写 `pending`，回 `ADD_RSP`。
- `handle_accept`：清 `pending`；`accept` 时双向写入 `friends`。
- `handle_list`：拼 `"pid,name,level,online;..."` 字符串。
- `handle_del`：双向删除。

## 潜在问题

> 本文潜在问题已纳入总登记表 [潜在问题](../潜在问题.md)，由 `tools/gen-problems.js` 从 `problems.json` 自动生成。

<!-- PROBLEMS:START -->
| ID | 状态 | 优先级 | 归期 | 问题 | 处理/方向 |
|---|---|---|---|---|---|
<!-- PROBLEMS:END -->
