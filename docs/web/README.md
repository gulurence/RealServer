# RealServer 联网对战改造 · 设计文档总纲

> 把单机 Cocos2d-x 射击游戏（`client/soulofgun/samples/HelloCpp`）接入 RealServer，
> 实现「登录 → 大厅 → 组队房间 → 帧同步战斗 → 结算退房」+ 好友 + 观战 + 回放。
> 本目录为在线文档（Docsify 风格），按 **总纲 → 框架 → 模块 → 交互** 组织。

## 架构决策速览

| 维度 | 决策 |
|---|---|
| 进程 | 接入类（Gate/Login）+ 业务类（Global/Game/World 备用）+ 独立日志程序 |
| Actor | 10 种：Gate/Login/Friend/Room/Chat/Player/World/Battle/Spectator/Log |
| 战斗 | 守望先锋式帧同步，20Hz，C++ 权威模拟 + 客户端预测/和解（rollback） |
| 通信 | asio + lambda RPC（从 gRPC 全量迁移） |
| ID | `ActorID[64]=MachineID[24]+PID[16]+ActorIndex[24]`，自描述路由 |
| 日志 | Nats 总线 + 独立日志进程，按 `物理机ID+进程ID` 分文件 |
| 语言 | 服务器业务全 Lua（战斗帧同步核心 C++）；C++ 沿用现有风格，Lua 全 snake_case |

## 文档目录

- **框架**（`框架/`）— 引擎层基础设施设计：类型系统 / ID 与寻址 / RPC / 进程配置 / 通信拓扑 / 日志 / 风格
- **模块**（`模块/`）— 各 Actor 系统设计：登录 / 好友 / 房间 / 聊天 / 战斗 / 观战
- **交互**（`交互/`）— 系统间消息流转与 RPC 路径
- 完整决策清单见 [总设计文档](../multiplayer-design.md)

## 潜在问题约定

每个文档末尾的 `## 潜在问题` 章节，记录该系统的**风险点 / 未决项 / 待迭代项**（bug 风险、性能隐患、边界情况、后续优化），供持续迭代对照。

**动态同步机制**（避免散落漂移）：

- **唯一数据源**：`problems.json`（每条 = ID/分组/模块/状态/优先级/归期/标题/处理方向）。
- **生成脚本**：`tools/gen-problems.js`（Node，零依赖）。改 `problems.json` 后跑：
  ```bash
  cd docs/web && node tools/gen-problems.js
  ```
- 脚本会 ① 重建总登记表 `潜在问题.md`；② 刷新各模块文档里 `<!-- PROBLEMS:START/END -->` 之间的表格。
- **不要手改**各文档的 `## 潜在问题` 章节——改 `problems.json` 再跑脚本即可。
- 状态取值：`done`（✅ 已处理）/ `mitigated`（⚠️ 已缓解）/ `open`（🔴 仍开放）。
