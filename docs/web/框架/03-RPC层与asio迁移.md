# RPC 层与 asio 迁移

> M0 基建项：把进程间 RPC 从 **gRPC** 全量迁移到 **asio + lambda**（TCP，超时 TimeOut）。
> 决策依据见 `docs/multiplayer-design.md` §2.5/§9/§16-18；命名风格见 [07-编码命名风格](07-编码命名风格.md)。

---

## 1. 现状评估

迁移前 gRPC 分布在四层：

| 层 | 位置 | 内容 | 迁移后去向 |
|---|---|---|---|
| 服务定义 | `source/proto-def/rpc/actor_mgr.proto`（`service ActorMgrService`）、`myservice.proto` | protoc + grpc_cpp_plugin 生成 `.grpc.pb.cc/.h` | 服务定义退役，仅保留消息结构 `PBRPC*`（经 `--cpp_out` 生成 `.pb.cc/.h`） |
| 服务端 | `engine/xActor/ActorMgrService.cpp/.h`（`ActorMgrServiceImpl`）、`engine/xRpc/xRpc.cpp` 的 `SRpcService` | gRPC `Service` + `ServerBuilder` | `engine/xRpc/CRpcTcpServer` + `engine/xRpc/ActorRpcHandlers.cpp`（methodId → lambda） |
| 客户端/channel | `engine/xRpc/xRpc.cpp` 的 `CRpcService`（`grpc::CreateChannel` 池 + 轮询取 channel）、`engine/xCluster/xLinkManager.cpp`（gRPC channel 建立） | channel 池 + stub 调用 | `engine/xRpc/CRpcTcpClient`（每 peer 一条 TCP 连接，`Call/CallSync/CallSyncRaw`） |
| 异步等待原语 | `engine/xRpc/xRpc.h` 的 `RpcCallAwaiter/AsyncRpcCall` | 基于 `grpc::Stub` 同步调用 + `RpcCallMgr` 阻塞工作线程 | 重写为 asio+lambda 版（直接异步调 `CRpcTcpClient::Call`，恢复走调度器 io_context） |

调用方（迁移后全部走 asio-TCP）：

- `engine/xCluster/CActorLoadManager.cpp`：`CallSync<actormgr::PBRPC*>`（CreateActor/FindActor/UninstallActor）+ `CallSyncRaw`（Login 链路）。
- `engine/xCluster/xLinkManager.cpp`：`CRpcTcpClient::Connect` 建链。
- `engine/xEnv/ProcessEnv.cpp`：启动时 `RegisterActorRpcHandlers()` + `CRpcTcpServer::Start` + `CRpcTcpClient::Init`。
- Lua 侧 `si_actor_creator_rpc_*` / `si_login_rpc`（`engine/luacore/luacore.i`）→ 委托 `CActorLoadManager`。

> 说明：本迁移代码主体（`RpcFrame.h`/`CRpcTcpClient`/`CRpcTcpServer`/`ActorRpcHandlers`）已就位；
> 本次收尾动作 = 摘除 `xRpc.h/.cpp` 里残留的 gRPC 类型、删除 gRPC service 实现、摘除 CMake 里的 gRPC 依赖与 `.grpc.pb.cc` 编译项。

---

## 2. 迁移方案

**目标**：保留接口语义不变（`CallSyncRaw`/methodId 语义、超时 code=2、无连接/未知方法 code=1），把传输层换成 asio-TCP。

1. **帧格式自建**（`engine/xRpc/RpcFrame.h`）：16 字节定长头 + protobuf body，见 §4。
2. **服务端**：`CRpcTcpServer`（单线程 io_context + acceptor + 每连接一协程），`Register(methodId, lambda)` 注册 handler，`ActorRpcHandlers.cpp` 注册 4 个跨进程 Actor 方法。
3. **客户端**：`CRpcTcpClient`（单线程 io_context + 每 peer 一条连接 + 每连接一读协程），`requestId` 单调自增反查回调。
4. **异步原语**：`RpcCallAwaiter/AsyncRpcCall` 从 gRPC stub 版改为 asio+lambda 版，`await_suspend` 直接异步 `Call`，完成后 `asio::post` 回调度器 io_context 恢复协程。
5. **摘除 gRPC**：删 `SRpcService`/`CRpcService`/`ActorMgrServiceImpl`，CMake 不再 `find_package(gRPC)`、不再链接 `gRPC::grpc++`、不再编译 `*.grpc.pb.cc`。

**未在本次一步完成的**（见 §8 潜在问题）：`make.bat` 的 grpc_cpp_plugin、`rpc/*.proto` 的 `service` 定义清理、`source/proto-cpp/**/*.grpc.pb.*` 生成物删除。

---

## 3. asio + lambda 架构

### 3.1 服务端 `CRpcTcpServer`

- 自持 `io_context`（`thread_proc` 里 `ioc.run()`），与 Actor 调度线程隔离。
- `Listener` 协程 `async_accept`，每连接 `co_spawn` 一个 `Session` 协程。
- `Session` 读头 → 读 body → 按 `methodId` 查 `m_mapHandlers` → 调 lambda handler → 组响应帧写回。
- handler 签名 `RpcHandler = std::function<RpcHandlerResult(const std::string& reqBody)>`，返回 `{errCode, rspBody}`。

### 3.2 客户端 `CRpcTcpClient`

- 自持 `io_context`（`thread_proc` 里 `make_work_guard` + `run()`）。
- `Connect(peerId, host, port)`：`asio::post` 到 io_context 里同步 connect，成功后登记 `m_connections[peerId]` 并 `co_spawn` 读协程 `Session`。
- `Submit(peerId, methodId, reqBody, cb)`：分配 `requestId` → 登记 `m_pending[requestId]` → 组帧 `asio::post` 写入 socket；无连接立即回调 `cb(1, "")`。
- `Session` 读帧，按 `requestId` 反查 `m_pending`，取回调执行（响应帧第三字段即 `errCode`）。
- 对外三层接口：
  - `Call<Req,Rsp>(peerId, methodId, req, cb)`：异步，protobuf 序列化/反序列化 + lambda 回调。
  - `CallSync<Req,Rsp>(...)`：`std::promise/future` 同步阻塞，超时返回 `code=2`（供 Lua `si_*` 同步接口）。
  - `CallSyncRaw(peerId, methodId, reqBody, timeoutMs)`：原始字节，非 protobuf（Login 链路用）。

### 3.3 与调度器 io_context 的接入

- 网络收发跑在 RPC 客户端/服务端**自己的 io_context 线程**，绝不阻塞 `CActorScheduler` 的调度线程。
- 协程等待原语 `RpcCallAwaiter::await_suspend` 里，回调在客户端 io_context 线程执行，完成后 `asio::post(m_ioc, [h]{ h.resume(); })` 把协程恢复**切回调度器 io_context**（`CActorScheduler::instance()->GetContext()`），保证 Actor 业务逻辑始终在单一调度线程内串行。
- 当前实际调用方（`CActorLoadManager`、Lua `si_*`）走同步 `CallSync`，阻塞发生在发起调用线程；协程内需异步时用 `co_await AsyncRpcCall<...>`。

---

## 4. 帧格式（`engine/xRpc/RpcFrame.h`）

主机字节序，与 `xNetAgent` 的 `NetPackageHeadST` 一致（同机集群场景）：

```
请求:  [uint32 bodyLen][uint32 requestId][uint32 methodId][uint32 fromProcessId][protobuf body]
响应:  [uint32 bodyLen][uint32 requestId][uint32 errCode  ][uint32 fromProcessId][protobuf body]
```

| 字段 | 请求 | 响应 |
|---|---|---|
| `u32Len` | body 字节数（不含 16 字节头） | 同左 |
| `u32RequestId` | 单调自增，关联请求/响应 | 原样回传 |
| `u32MethodId` | method id | errCode（0=ok，其余见 §5） |
| `u32FromProcessId` | 发送方进程 ID（服务端据此识别对端，供链路追踪 Recv 方向） | 本进程 ID |

- `RpcFrameHeadST` 为 `#pragma pack(1)`，`RpcFrameHeadSize = 16`。
- 单帧 body 上限 `MAX_RPC_BODY_SIZE = 4MB`。

**methodId 语义**（`RpcMethodId`）：

| methodId | 语义 | 请求/响应消息 |
|---|---|---|
| 1 | `RPC_METHOD_CREATE_ACTOR` | `PBRPCCreateActorRequest/Reply` |
| 2 | `RPC_METHOD_FIND_ACTOR` | `PBRPCFindActorRequest/Reply` |
| 3 | `RPC_METHOD_UNINSTALL_ACTOR` | `PBRPCUninstallActorRequest/Reply` |
| 4 | `RPC_METHOD_LOGIN` | body = `[1 字节 op][protobuf 负载]`（Gate→Login） |

---

## 5. 超时 / 错误码

| code | 语义 | 触发点 |
|---|---|---|
| 0 | 传输成功 | 响应头 `errCode == 0`，`ok=true` |
| 1 | 无连接 / 未知方法 | 客户端无该 peer 连接（`CRpcTcpClient::Submit`）；服务端 `m_mapHandlers` 未注册该 methodId；handler 内部业务失败 |
| 2 | 超时 TimeOut | `CallSync`/`CallSyncRaw` 用 `std::future::wait_for(timeoutMs)` 超时（默认 5000ms） |

- 业务错误（如「该进程不承载此 Actor 类型」「创建失败」）在**响应 protobuf 的 `error_code` 字段**里表达（如 `PBRPCCreateActorReply.error_code`），传输层 `errCode` 仍为 0。
- 与原 gRPC 版语义一致：超时 `code=2`、无连接 `code=1` 均未变。

---

## 6. 接口兼容性

| 原 gRPC 接口 | 迁移后 | 说明 |
|---|---|---|
| `CRpcService::GetChannel/ConnectToServer/ConnectToLocalServer` | `CRpcTcpClient::Connect/Call/CallSync/CallSyncRaw` | channel 池 → 每 peer 一条连接 |
| `SRpcService::Start/RegistService` | `CRpcTcpServer::Start/Register` | gRPC `Service` → `methodId → lambda` |
| `ActorMgrServiceImpl`（CreateActor/FindActor/UninstallActor） | `ActorRpcHandlers.cpp` 的 3 个 lambda handler | 语义一致（复用 `actor_mgr.proto` 消息） |
| `RpcCallAwaiter/AsyncRpcCall`（gRPC stub） | asio+lambda 版 | 签名从 `(stub, fn, req, ioc)` 变为 `(peerId, methodId, req, ioc)` |
| `RpcCallMgr` | 保留 | 遗留通用工作线程池，仅 `xNats/xService` 遗留宏引用，无 gRPC 依赖 |

`CallSyncRaw` 语义与 `methodId` 1/2/3/4 完全保留，`CActorLoadManager.cpp` 与 Lua 侧无需改动。

---

## 7. 改动清单

| 文件 | 改动 |
|---|---|
| `engine/xRpc/xRpc.h` | 移除 gRPC（`grpcpp` 头、`RpcResult`、`SRpcService`、`CRpcService`、gRPC 版 `RpcCallAwaiter/AsyncRpcCall`）；保留 `RpcCallMgr`；新增 asio+lambda 版 `RpcCallAwaiter/AsyncRpcCall` |
| `engine/xRpc/xRpc.cpp` | 移除 `SRpcService`/`CRpcService` 实现；保留 `RpcCallMgr` 实现 |
| `engine/xActor/ActorMgrService.cpp/.h` | 删除（gRPC service，被 `ActorRpcHandlers` 取代） |
| `engine/xCluster/xProcessManager.cpp` | 移除 `#include "xRpc/xRpc.h"`、`#include "xActor/ActorMgrService.h"`；更新 `InitEnvironment` 注释 |
| `engine/xCluster/xProcessManager.h` | 更新 `InitEnvironment` 注释 |
| `engine/xCluster/xLinkManager.h` | 类注释/`DiscoverAndConnect` 注释 gRPC → asio-TCP |
| `engine/xCluster/xProcessDefine.h` | `PeerConnection::channelName` 注释 gRPC channel → asio 连接标识 |
| `engine/CMakeLists.txt` | 不再编译 `*.grpc.pb.cc`；移除 `find_package(gRPC)`、`gRPC::grpc++`/`PkgConfig::GRPC` 链接与 include |

> 以下为「已就位、本次未改」：`engine/xRpc/RpcFrame.h`、`CRpcTcpClient.*`、`CRpcTcpServer.*`、`ActorRpcHandlers.*`、`engine/xCluster/CActorLoadManager.*`（任务限定不动）、`engine/xEnv/ProcessEnv.cpp`。

---

## 潜在问题

> 本文潜在问题已纳入总登记表 [潜在问题](../潜在问题.md)，由 `tools/gen-problems.js` 从 `problems.json` 自动生成。

<!-- PROBLEMS:START -->
| ID | 状态 | 优先级 | 归期 | 问题 | 处理/方向 |
|---|---|---|---|---|---|
| **A1** | ✅ 已处理 | 高 | M0 | CallSync 超时未清理 m_pending | Submit/Call 改返 uint32 requestId（无连接返 0），CallSync/CallSyncRaw 超时后 m_pending.erase(requestId)，消除迟到响应触发 future_error 与 pending 泄漏。 |
| **A2** | ✅ 已处理 | 低 | M0 | proto 的 service 定义未清理 | actor_mgr/myservice 两个 proto 已删 service 块、保留 PBRPC* 消息，protoc 重跑验证通过。 |
| **A3** | ✅ 已处理 | 低 | M0 | stale *.grpc.pb.cc/.h 生成物未删 | 已删 4 个 .grpc.pb.cc/.h，rpc 目录现只剩 *.pb.cc/.h。 |
| **A4** | ✅ 已处理 | 低 | M0 | luacore_wrap.cxx 过时生成物 | 仓库旧文件已删，构建时由 SWIG 从 engine/luacore/luacore.i 重新生成。 |
| **A5** | ✅ 已处理 | 低 | M1 | grpcPort 字段名未 rename rpcPort | 已统一 rename 为 rpcPort，波及 xServerConfig/xLocalInfo/xProcessDefine/xProcessManager/xLinkManager/xServiceDiscovery/ProcessEnv 等文件；源码已无 grpcPort 残留。 |
| **A6** | 🔴 仍开放 | 低 | — | RpcCallMgr/xNatsCall.h 遗留宏混 std::cout | 为兼容 xService/xNats 旧骨架保留，其宏内混 std::cout 违反日志规范；待旧骨架清理时一并移除。 |
| **A7** | 🔴 仍开放 | 中 | — | CallSync 阻塞调用方线程 | 同步 CallSync 在 Actor 协程内直接调用会阻塞调度线程最多 timeoutMs；协程场景应改 co_await AsyncRpcCall（原语已就绪），后续按需替换调用点。 |
<!-- PROBLEMS:END -->
