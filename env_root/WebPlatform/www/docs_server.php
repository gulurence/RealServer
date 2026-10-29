<?php
// =============================================================================
// 服务端架构文档 — RealServer
//
// 技术文档模块 / 服务端。静态文档页，无动态数据。
// 覆盖: 框架定位、进程模型、一级模块(engine/)、协议与数据链路。
// =============================================================================
require_once __DIR__ . '/_layout.php';

layout_header('服务端架构', 'docs_server');
?>
<div class="summary">
    <div class="stat-grid">
        <div class="stat"><div class="k">语言</div><div class="v">C++20<small>单可执行文件</small></div></div>
        <div class="stat"><div class="k">并发模型</div><div class="v">Actor<small>+ 协程调度器</small></div></div>
        <div class="stat"><div class="k">热逻辑</div><div class="v">Lua<small>SWIG 绑定</small></div></div>
        <div class="stat"><div class="k">服务发现</div><div class="v">Redis<small>gRPC 互联</small></div></div>
    </div>
    <p>
        RealServer 是一个 C++20 分布式游戏服务器框架：<b>单可执行文件、多进程部署</b>，核心是
        <b>Actor 模型 + Lua 脚本热逻辑（SWIG 绑定）</b>，进程间通过 gRPC + Redis 服务发现组成集群。
        进程身份由 <code class="doc-path">config/server.xml</code> 的 <code>&lt;process type&gt;</code> 决定。
    </p>
</div>

<!-- ================= 进程模型 ================= -->
<div class="card">
    <div class="card-header">进程模型 <span class="dim">进程身份由 config/server.xml 决定</span></div>
    <div class="card-body">
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>type</th><th>进程</th><th>承载 Actor</th><th>职责</th></tr></thead>
            <tbody>
                <tr><td>100</td><td>Gate</td><td>EGate</td><td>网关：客户端 TCP 接入、鉴权、协议路由</td></tr>
                <tr><td>101</td><td>Login</td><td>ELogin</td><td>登录 / 选角 / 创角</td></tr>
                <tr><td>102</td><td>Global</td><td>EFriend · EChat · EConfig（单例）</td><td>全局服务：好友 / 聊天 / 配置热更</td></tr>
                <tr><td>201</td><td>Game</td><td>EPlayer · ERoom</td><td>游戏承载：玩家 / 房间</td></tr>
                <tr><td>202</td><td>Battle</td><td>EBattle · ESpectator（多实例）</td><td>战斗承载：帧同步战斗 / 观战（客户端直连）</td></tr>
            </tbody>
        </table></div>
        <div class="dim">
            多进程通过 Redis 服务发现 + gRPC 互联组成集群；Actor 跨进程创建由
            <code>ActorCreator</code> 完成，<code>CActorMgr</code> 以两级索引 <code>type → (id → actor)</code> 管理。
        </div>
    </div>
</div>

<!-- ================= 一级模块 ================= -->
<div class="card">
    <div class="card-header">一级模块 <span class="dim">engine/ 框架分层</span></div>
    <div class="card-body">
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>模块</th><th>职责</th></tr></thead>
            <tbody>
                <tr><td>xActor/</td><td>Actor 模型核心：CActorMgr 两级索引、ActorCreator 跨进程创建</td></tr>
                <tr><td>xCluster/</td><td>进程集群：服务发现（Redis）、链接管理（asio-TCP RPC）、心跳、负载</td></tr>
                <tr><td>tService/</td><td>第三方服务管理（Redis/MySQL/Log/Xml/Platform），引用计数共享复用</td></tr>
                <tr><td>xRecord/</td><td>持久化：读走 Redis 缓存 miss 回源 DB，写走 Redis 同步 + DB 异步</td></tr>
                <tr><td>xDatabase/ xRedis/</td><td>数据库 / Redis 基础设施</td></tr>
                <tr><td>xNet/</td><td>网络：CNetGateway 帧协议（与客户端一致）</td></tr>
                <tr><td>xShareMem/ xConfig/ xLog/</td><td>共享内存 / 配置（XML）/ 日志（spdlog 封装）</td></tr>
                <tr><td>luacore/</td><td>SWIG 接口（.i）生成 Lua 绑定，导出 si_* 前缀 C 接口</td></tr>
                <tr><td>main.cpp</td><td>进程入口，11 阶段启动流程</td></tr>
            </tbody>
        </table></div>
    </div>
</div>

<!-- ================= 协议与链路 ================= -->
<div class="card">
    <div class="card-header">协议与数据链路</div>
    <div class="card-body">
        <h3 class="doc-h3">源码组织</h3>
        <pre class="doc-pre"><span class="c">source/proto-def/</span>   <span class="m">.proto 源定义</span>
      ↓ protoc（make.bat）
<span class="c">source/proto-cpp/</span>     <span class="m">protoc 生成物（勿手改）</span>
<span class="c">script/</span>                <span class="m">Lua 业务：common / actors(gate·login·player·world) / {dev,test,prod}</span>
<span class="c">shared/</span>                <span class="m">C/S 共享：battlecore（确定性战斗）+ config（配置）</span></pre>
        <h3 class="doc-h3">客户端接入链路</h3>
        <pre class="doc-pre">客户端 TCP ──▶ Gate(100) ──路由──▶ Login(101) ──▶ Global(102) / Game(201)
                     │  帧协议 [uint32 bodyLen][protobuf body]
                     │  body 前两字段 big_type(1)/small_type(2) 路由到对应 Actor
                     ▼
              WebPlatform(api/login.php) —— 账号密码校验（HTTP，签发 session token）</pre>
        <h3 class="doc-h3">架构约束</h3>
        <div class="table-responsive"><table class="table table-vcenter doc-table">
            <thead><tr><th>约束</th><th>说明</th></tr></thead>
            <tbody>
                <tr><td>业务逻辑在 Lua</td><td>具体玩法逻辑写 script/，C++ 只做框架与基础设施</td></tr>
                <tr><td>DB 读写走 CRecordMgr</td><td>Redis 缓存 + DB 异步落库，禁止业务层直接拼 SQL</td></tr>
                <tr><td>生成物勿改</td><td>source/proto-cpp/、engine/luacore_generated/ 为生成物；改 .proto 后重跑 make.bat</td></tr>
                <tr><td>配置是 XML</td><td>config/server.xml（非 JSON/.env），运行时相对路径加载</td></tr>
            </tbody>
        </table></div>
    </div>
</div>

<?php layout_footer(); ?>
