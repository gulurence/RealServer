================================================================================
 RealServer —— C++20 分布式游戏服务器框架
================================================================================

RealServer 是一个单可执行文件、多进程部署的分布式游戏服务器框架。

核心设计：
  * Actor 模型         —— 业务以 Actor 为单元，跨进程创建 / 路由 / 负载均衡
  * Lua 脚本热逻辑     —— 玩法逻辑全部写在 Lua（SWIG 生成绑定），支持热更
  * 进程集群           —— 服务发现（Redis）+ 进程间链接 + 心跳 + 负载
  * 协程调度器         —— C++20 coroutine + 同步双线程池
  * 持久化             —— 读走 Redis 缓存 miss 回源 DB，写走 Redis 同步 + DB 异步

进程身份由 config/server.xml 的 <process type> 决定，同一份二进制按配置
扮演不同角色，通过 Redis 服务发现组成集群。


================================================================================
 进程 / Actor 类型
================================================================================

进程类型（EProcessType，xBase/xTypeDefine.h）：

    type  进程名    承载的 Actor 类型
    ----  --------  --------------------------------------------
    100   Gate      网关 Actor（客户端接入 / 消息转发）
    101   Login     登录 Actor（账号校验 / 会话）
    102   Global    好友 / 聊天 / 配置 Actor
    201   Game      玩家 / 房间 Actor
    202   Battle    战斗 / 观战 Actor（帧同步, 客户端直连）

Actor 类型（EActorType）：

    AGate(100)  ALogin(101)  AFriend(102)  ARoom(103)  AChat(104)
    AConfig(105)  APlayer(201)  ABattle(203)  ASpectator(204)

每类进程预留 50 个 RPC 端口区间：
    Gate 50551-50600 / Login 50601-50650 / Game 50651-50700 / Battle 50701-50750 / Global 50751-50800


================================================================================
 目录结构
================================================================================

RealServer/
├── CMakeLists.txt          # 主 CMake 配置（add_subdirectory(engine)）
├── config/                 # 运行时配置（XML）
│   └── server.xml          # 进程身份 / Redis / MySQL 连接池（勿提交真实凭证）
├── engine/                 # 引擎框架（全部 C++，单可执行文件产物）
│   ├── main.cpp            # 进程入口，11 阶段启动流程
│   ├── xActor/             # Actor 模型核心（CActorMgr 两级索引 type→(id→actor)）
│   ├── xCluster/           # 进程集群：服务发现 / 链接管理 / 心跳 / 负载
│   ├── xScheduler/         # 协程 + 双线程池调度器
│   ├── xEvent/             # 协议分发（按 (msgType<<32)|msgID 注册回调）
│   ├── xRpc/               # asio-TCP 进程间 RPC
│   ├── xRecord/            # 持久化（Redis 缓存 + DB 异步落库）
│   ├── tService/           # 第三方服务管理（Redis/MySQL/Log/Xml/Platform，引用计数复用）
│   ├── xDatabase/ xRedis/ xNats/ xNet/ xShareMem/ xConfig/ xLog/ xBase/
│   ├── luacore/            # SWIG 接口（.i）→ 生成 Lua 绑定，导出 si_* 前缀 C 接口
│   ├── luacore_generated/  # SWIG 生成物（勿手改）
│   └── node_manager/       # NodeManager 物理机守护进程（独立轻量二进制）
├── script/                 # Lua 业务逻辑
│   ├── common/             # 类系统 / Actor 基类 / RPC / Redis / DB / 热更
│   ├── actors/             # gate / login / player / room / battle / ... 各 Actor
│   └── {dev,test,prod}/    # 每平台一套 init.lua
├── source/
│   ├── proto-def/          # .proto 源定义（msg/ + rpc/）
│   └── proto-cpp/          # protoc 生成物（*.pb.cc/*.pb.h，勿手改）
├── shared/                 # 客户端/服务器同源共享代码
│   ├── battlecore/         # 帧同步战斗核心
│   └── config/             # 共享配置读写
├── tools/                  # Go 代码生成器（gorm/proto/redis/nats/pool 模板）
├── third_part/             # 第三方依赖（被 .gitignore 忽略，构建前需备好）
├── bin/windows/            # Windows 部署目录（exe 编译后自动拷贝）
├── document/  docs/        # 文档 / 架构图 / 编码规范
└── build/                  # 构建目录


================================================================================
 构建
================================================================================

Windows（MSVC）：

    cmake -B build -G "Visual Studio 18 2026" -A x64
    cmake --build build

    产物：build/engine/RealServer（编译后自动拷贝到 bin/windows/RealServer.exe）

Linux（依赖系统包 + CMake 源码编译 nats/redis++）：

    cmake -B build
    cmake --build build

    依赖：pkg-config 的 mysql/mariadb、libxml2、hiredis、protobuf

注：third_part/ 不在版本库内，构建前需先备好第三方依赖
（Windows 用 vcpkg/预装，Linux 用系统包 + CMake 源码编译 nats.c 与 redis-plus-plus）。


================================================================================
 运行
================================================================================

    ./engine/RealServer

    Windows 部署：cd bin/windows && .\start_cluster.ps1（或 start_cluster.bat）

重要：工作目录必须同时包含 config/ 和 script/，代码按相对路径加载两者。

无 Redis 环境可走本地回退模式（server.xml 的 <cluster><local> 段），
进程以静态 peer 列表互联，不需要服务发现。


================================================================================
 协议生成
================================================================================

修改 source/proto-def/ 下的 .proto 后，重新生成 C++：

    cd source/proto-def && ./make.bat

生成物落到 source/proto-cpp/（*.pb.cc / *.pb.h），禁止手改生成文件。

注意：make.bat 中 grpc_cpp_plugin 路径写死为 D:/vcpkg-master/...，
需按本机路径调整；该文件为 GBK 编码。


================================================================================
 开发约定（摘要，详见 document/ 与 CLAUDE.md）
================================================================================

  * 业务逻辑写在 Lua（script/），C++ 只做框架与基础设施，不写具体玩法。
  * 数据库读写走 CRecordMgr（Redis 缓存 + DB 异步落库），禁止业务层拼 SQL。
  * 整数类型用 xBase/xDefine.h 的 int8/int16/int32/int64/uint8/... 别名，
    禁止裸写 int/long。
  * 日志走 XLOG/XERR/XWRN（集群用 XCLOG/XCERR/XCWRN），禁止 std::cout/printf。
  * 单例用 xSingleton<T>；文件名与类名同名（xActor/Actor.h ↔ CActor）。
  * 类名前缀：x*（框架基类）、C*（管理器）、t*（第三方服务）。


