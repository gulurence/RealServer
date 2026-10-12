# 项目名：RealServer

一个 C++20 分布式游戏服务器框架。单可执行文件（`RealServer`）、多进程部署，核心是 **Actor 模型 + Lua 脚本热逻辑（SWIG 绑定）**，进程间通过 gRPC + Redis 服务发现组成集群。

## Commands

你需要知道、但无法从构建文件一眼看出的命令：

- `cmake -B build -G "Visual Studio 18 2026" -A x64` — 配置构建（Windows/MSVC）
- `cmake --build build` — 编译，产物在 `build/engine/RealServer`
- `cd source/proto-def && ./make.bat` — 重新生成 proto（protoc → `source/proto-cpp`）
- `./engine/RealServer` — 运行（**工作目录必须同时包含 `config/` 和 `script/`**，因为代码里用相对路径加载）

Linux 构建走另一分支：`cmake -B build && cmake --build build`（依赖 pkg-config 的 mysql/mariadb、libxml2、hiredis、protobuf、grpc；nats.c 与 redis-plus-plus 由 CMake 源码编译）。

## Code Style

和默认 C++ 风格有差异的约定：

- 整数类型用 `xBase/xDefine.h` 里定义的别名 `int8/int16/int32/int64/uint8/uint16/uint32/uint64`，禁止直接写裸 `int`/`long`
- 字符缓冲区声明需零初始化：`char buf[64]={0};`（禁止裸 `char buf[64];`）
- 单例统一用 `xSingleton<T>` 模板（`xBase/xSingleton.h`），访问方式历史上是 `getMe()`，新代码多用 `instance()`
- 日志统一走 `xLog` 的宏 `XLOG/XERR/XWRN`（spdlog 封装），集群相关用 `XCLOG/XCERR/XCWRN`；**禁止 `std::cout`/`printf`**
- 注释以中文为主（新代码），类名带前缀约定：`x*`（框架基类）、`C*`（较新的管理器，如 `CActorMgr`、`CProcessManager`）、`t*`（第三方服务 `tService*`）
- 文件名与类名同名（`xActor/Actor.h` ↔ `CActor`）

## Architecture

关键架构约束：

- `engine/` — 引擎框架，全部 C++，按模块划分：
  - `xActor/` — **Actor 模型核心**（`CActorMgr` 两级索引 `type→(id→actor)`，`ActorCreator` 跨进程创建）
  - `xCluster/` — 进程集群：服务发现（Redis）、链接管理（gRPC）、心跳、负载
  - `xScheduler/` — 协程（C++20 coroutine）+ 同步双线程池调度器
  - `xEvent/` — 协议分发（`xEventDispatcher` 按 `(msgType<<32)|msgID` 注册回调）
  - `tService/` — 第三方服务管理（Redis/MySQL/Log/Xml/Platform，**引用计数共享复用**）
  - `xRecord/` — 持久化：读走 Redis 缓存 miss 回源 DB，写走 Redis 同步 + DB 异步
  - `xDatabase/ xRedis/ xNats/ xNet/ xShareMem/ xConfig/ xLog/` — 基础设施
  - `luacore/` — SWIG 接口（`.i`）生成 Lua 绑定，导出 `si_*` 前缀 C 接口
  - `main.cpp` — 进程入口，11 阶段启动流程
- `source/proto-def/`（`.proto` 源定义）→ `source/proto-cpp/`（protoc 生成物，**勿手改**）
- `script/` — Lua 业务：`common/`（类系统/Actor 基类/RPC/Redis/DB）、`actors/`（gate/login/player/world）、`{dev,test,prod}/` 每平台一套 `init.lua`
- `tools/` — Go 编写的代码生成器（gorm/proto/redis/nats/pool 模板）
- 单可执行文件，进程身份由 `config/server.xml` 的 `<process type>` 决定：`100=Gate` `101=Login` `201=Player` `202=World`

## Rules

红线，用了强调词：

- NEVER 提交包含真实密码、API 密钥、连接字符串的代码（`config/server.xml` 含 DB/Redis 明文连接信息，注意不要填生产真实凭证）
- ALWAYS 修改 `.proto` 后重新运行 `make.bat` 生成 C++，禁止手改 `source/proto-cpp/*.pb.cc` / `*.grpc.pb.cc`
- ALWAYS 业务逻辑写在 Lua（`script/`），C++ 只做框架与基础设施，不写具体玩法逻辑
- 数据库读写走 `CRecordMgr`（Redis 缓存 + DB 异步落库），禁止在业务层直接拼 SQL

## Gotchas

那些代码里看不出来的坑：

- 配置是 **XML**（`config/server.xml`），不是 JSON 也不是 `.env`；运行时相对路径加载 `config/server.xml` 和 `script/`
- `main.cpp` 主循环是 `while(true)` 死循环，后面的优雅关闭代码**实际不可达**（代码注释已自认 unreachable）
- 两套服务框架并存：`xService/` 是遗留/未完成的旧骨架（`ServiceLogin::Create()` 返回 `nullptr`），当前标准是 `tService/`，不要在新代码里依赖 `xService`
- `third_part/` 被 `.gitignore` 忽略，构建前需先备好依赖（Windows 用 vcpkg/预装第三方，Linux 用系统包 + CMake 源码编译 nats/redis++）
- `source/proto-def/make.bat` 里 `grpc_cpp_plugin` 路径**写死**为 `D:/vcpkg-master/...`，需按本机路径调整；且该文件是 GBK 乱码，注意编码
- Redis 不可用时可走本地回退配置（`server.xml` 的 `<cluster><local>` 段），这是无 Redis 环境的测试模式
- `source/proto-cpp/` 和 `engine/luacore_generated/` 都是生成物，勿直接编辑
