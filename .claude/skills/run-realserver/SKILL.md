---
name: run-realserver
description: 构建 / 启动 / 驱动 RealServer (C++20 分布式游戏服务器, Actor 模型 + Lua)。当需要 run、start、build、launch、verify RealServer 或单个进程 (Gate/Login/Player/World)、确认它到达 READY 时使用。
---

# run-realserver

RealServer 是一个**单可执行文件、多进程**的 C++20 游戏服务器框架：进程身份由
`config/server.xml` 的 `<process type>` 决定（100=Gate 101=Login 201=Player 202=World），
启动后是一个"类型化 Actor 运行容器"，业务逻辑全在 Lua（`script/`）。

它是**长驻服务器，没有 HTTP 接口**。驱动方式 = 后台启动 + 抓日志确认 `RealServer READY`
+ 检查 gRPC/网关监听端口 + 向网关发一次 TCP 连接 + 停掉。入口是下面的 `driver.sh`。

> 本文件所有路径相对**仓库根目录**（`D:\code\RealServer`）。driver 在
> `.claude/skills/run-realserver/driver.sh`，从仓库根目录任意位置调用。

## Prerequisites

- Windows 11 + Visual Studio 2026（generator `Visual Studio 18 2026`），CMake ≥ 3.10。
- vcpkg 依赖已装到 `D:/code/vcpkg/realserver-deps/installed/x64-windows`
  （grpc 1.70 / protobuf 5.29 / hiredis / abseil）。
- Git Bash（`bash`）——driver 是 bash 脚本，且用 `/dev/tcp` 做连接探测。
- 运行期 DLL 三个目录必须可被加载（driver 已自动加进 `PATH`）：
  - `D:/code/vcpkg/realserver-deps/installed/x64-windows/debug/bin`
  - `third_part/mysql-connector-c-6.1.11-winx64/lib`（`libmysql.dll`）
  - `third_part/libxml2/build_win/Debug`（`libxml2d.dll`）

## Build

```bash
.claude/skills/run-realserver/driver.sh build
```

等于：`build_msvc` 已配置则直接 `cmake --build build_msvc --config Debug`；未配置时先
`cmake -B build_msvc -G "Visual Studio 18 2026" -A x64`。产物是
`build_msvc/engine/Debug/RealServer.exe`（Lua 5.4 与 SWIG 绑定源码编进 exe，无 Lua DLL）。

## Run（agent 路径）— 用 driver

```bash
.claude/skills/run-realserver/driver.sh up       # 后台启动, 等 READY, 打印监听端口
.claude/skills/run-realserver/driver.sh status   # RUNNING/NOT RUNNING + 监听端口 + 日志尾部
.claude/skills/run-realserver/driver.sh probe    # 向网关端口发一次 TCP 连接, 验证网络层接受客户端
.claude/skills/run-realserver/driver.sh logs     # tail 日志 (Ctrl-C 退出)
.claude/skills/run-realserver/driver.sh down     # 停止 (taskkill 按镜像名)
```

一次完整的"改动是否生效"验证流程：

```bash
.claude/skills/run-realserver/driver.sh build
.claude/skills/run-realserver/driver.sh up
.claude/skills/run-realserver/driver.sh probe     # 期望输出 "OK — 网关 :100 接受连接并记录断开"
.claude/skills/run-realserver/driver.sh down
```

**判定标准**：`up` 打印 `READY` 即成功；`probe` 在日志里看到
`[NetGateway] client disconnect cid=1` 即证明网关层在真实接受/断开客户端连接。

## Run（human 路径）

在仓库根目录的终端里直接跑（同一套 DLL PATH）：

```bash
export PATH="/d/code/vcpkg/realserver-deps/installed/x64-windows/debug/bin:third_part/mysql-connector-c-6.1.11-winx64/lib:third_part/libxml2/build_win/Debug:$PATH"
./build_msvc/engine/Debug/RealServer.exe
```

前台死循环，Ctrl-C 停。**headless/CI 下没意义**——用上面的 `driver.sh up`。

## Test

没有独立测试套件。冒烟测试就是 `up` + `probe`：启动到 READY 且网关接受连接即通过。

## Gotchas

- **配置解析有 bug（`xBase/xXMLParser.cpp:257` 的 `getPropStr` 用 `out +=` 追加而非 `out =` 赋值）**，
  同元素多次读属性时字符串值会串接。后果：`<process name="gate-01" host="0.0.0.0" port="10000">`
  解析成 `name=100gate-01`、`host=100gate-010.0.0.0`、`listenPort=100`（`atoi("100gate-01...")=100`）。
  所以 **Gate 实际绑定的是端口 100，不是 10000**。日志里 `[NetGateway] listening on 0.0.0.0:100` 是真实行为，
  不是显示问题。`grpc` 端口不受影响（单独读、独立字符串），仍是 50051。
- **Redis 不可用是正常路径**：`RedisMgr.cpp:93 ... not find title pool connect [cluster_discovery]`
  是一条 `[error]` 级别日志，但服务器据此走 standalone 本地回退，照常到 READY。MySQL 同样懒加载、不连也照常启动。
- **端口 10000 可能被占用**：本机 `YunDetectService.exe` 监听 127.0.0.1:10000，但当前配置 bug 让 Gate 绑到 100，所以不冲突。
- `config/server.xml` 被 `.gitignore` 忽略（含明文 DB/Redis 连接信息，红线：别提交真实凭证）。
  干净检出时 driver 的 `up` 会自动从 `config/server.xml.example` 复制生成。
- `logs/` 目录**无需预建**——spdlog 的 rotating_file_sink 会自动创建；`logs/normal.log` 是服务器自己的滚动日志，
  driver 用 `logs/driver.log`（每次 `up` 清空重写）做 READY 判定，避免旧日志误判。
- `down` 用 `taskkill //F //IM RealServer.exe` 按镜像名杀进程，会杀掉**所有** RealServer 实例（单进程开发没问题）。
- 换机器时 vcpkg 前缀不同：engine/CMakeLists.txt 里 `VCPKG_INSTALLED_ROOT` 写死为
  `D:/code/vcpkg/realserver-deps/installed/x64-windows`，driver 可用环境变量 `VCPKG_BIN` 覆盖。

## Troubleshooting

| 症状 | 修复 |
|---|---|
| `error while loading shared libraries: libxml2d.dll: cannot open shared object file` | 缺少 DLL 路径，把 `third_part/libxml2/build_win/Debug`（及 mysql `lib/`、vcpkg `debug/bin`）加进 `PATH`；或直接用 `driver.sh up` 自动处理 |
| `up` 30s 内没打印 `READY` | `driver.sh logs` 看尾部；常见是缺 DLL 导致没起、或 `script/` 不在 cwd（driver 已在仓库根目录执行，别在别处 `cd` 后跑 exe） |
| 想确认网关端口 | 日志里 grep `listening on 0.0.0.0:`，或 `driver.sh status` 会读出来 |
| `probe` 报"日志未见 disconnect" | 连接成功但日志落盘有 1s 延迟，重跑一次 `probe`；`cid` 从 1 递增属正常 |
