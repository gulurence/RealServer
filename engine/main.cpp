#include <grpcpp/grpcpp.h>
#include "rpc/myservice.grpc.pb.h"
#include "msg/login.pb.h"
#include "rpc/myservice.pb.h"

#include <iostream>
#include <coroutine>
#include <queue>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>
#include <string>
#include <functional>

#include "xLog/xLog.h"
#include "xEvent/xEventDispatcher.h"
#include "xService/xServiceMgr.h"
#include "xRpc/xRpc.h"
#include "xActor/ActorMgr.h"
#include "luacore/LuaVM.h"

// New subsystem headers
#include "xConfig/xServerConfig.h"
#include "tService/tServiceMgr.h"
#include "tService/tThridService.h"
#include "xActor/ActorMgrService.h"

// Cluster / service discovery modules
#include "xCluster/xProcessDefine.h"
#include "xCluster/xProcessManager.h"
#include "xCluster/xServiceDiscovery.h"
#include "xCluster/xLinkManager.h"
#include "xErrorCode/xErrorCode.h"

int main() {
    // =========================================================================
    // Phase 1: 日志系统初始化
    // =========================================================================
    xLog::get_mutable_instance().Init();
    XLOG("[INIT] ============================================================");
    XLOG("[INIT] RealServer starting");
    XLOG("[INIT] ============================================================");

    // =========================================================================
    // Phase 2: 配置加载
    // =========================================================================
    XLOG("[INIT] Phase 2/11: Loading configuration");
    if (!CServerConfig::getMe().Load("config/server.xml")) {
        XERR("[INIT] Failed to load config/server.xml — using defaults");
    }
    auto& cfg = CServerConfig::getMe().GetConfig();
    XLOG("[INIT] Config: platform=%s grpc=%u process_type=%d process_name=%s",
         cfg.platform.c_str(), cfg.gRPCPort,
         cfg.process.type, cfg.process.name.c_str());

    // =========================================================================
    // Phase 3: 创建进程身份
    //   1. 从 Redis 获取配置 + 生成唯一进程 ID
    //   2. 失败则使用本地回退配置 (无 Redis 环境的测试模式)
    // =========================================================================
    XLOG("[INIT] Phase 3/11: Creating process identity");
    auto& procMgr = CProcessManager::getMe();

    // 初始化服务发现 (连接 Redis pool)
    auto& discovery = CServiceDiscovery::getMe();
    discovery.Init(cfg.discoveryRedis.poolTitle);

    // 尝试 Redis 模式创建
    EProcessType procType = (EProcessType)cfg.process.type;
    bool procCreated = procMgr.CreateProcess(procType, cfg.process.name,
                                              cfg.clusterFallback.local.processId);

    if (!procCreated) {
        // Redis 不可用 — 使用本地回退配置
        XLOG("[INIT] Using local fallback config for process identity");
        procMgr.CreateProcessFromFallback(cfg.clusterFallback.local);
    }

    auto& selfInfo = procMgr.GetSelfInfo();
    XLOG("[INIT] Process identity: type=%s(%d) id=%lld name=%s",
         ProcessTypeName(selfInfo.type), (int32)selfInfo.type,
         selfInfo.processId, selfInfo.processName.c_str());

    // =========================================================================
    // Phase 4: 初始化进程环境
    //   调度器 → gRPC 服务器 → 第三方服务
    // =========================================================================
    XLOG("[INIT] Phase 4/11: Initializing environment");

    // 4a. 调度器
#define CoroutineSchedulerCount 4
#define SyncSchedulerCount 4
    xSchedulerMgr::getMe().Init(CoroutineSchedulerCount, SyncSchedulerCount);

    // 4b. gRPC 服务器 + ActorMgrService 注册
    static ActorMgrServiceImpl s_actorMgrService;
    SRpcService::getMe().RegistService(&s_actorMgrService);
    SRpcService::getMe().Start(cfg.gRPCPort);

    // 4c. gRPC 客户端连接池 (本地回环)
    CRpcService::getMe().ConnectToLocalServer("local", 6, cfg.gRPCPort);

    // 4d. 异步 RPC 线程
    RpcCallMgr::getMe().Init();

    // 4e. 第三方服务配置
    if (!cfg.redisConfigs.empty()) {
        tServiceMgr::getMe().SetServiceConfig(EST_Redis, cfg.redisConfigs[0]);
    }
    if (!cfg.dbConfigs.empty()) {
        tServiceMgr::getMe().SetServiceConfig(EST_MySql, cfg.dbConfigs[0]);
    }
    {
        ThridServiceConfig logCfg;
        logCfg.log.logPath  = "logs/";
        logCfg.log.logLevel = "info";
        tServiceMgr::getMe().SetServiceConfig(EST_Log, logCfg);

        ThridServiceConfig xmlCfg;
        xmlCfg.xml.configPath = "config/server.xml";
        tServiceMgr::getMe().SetServiceConfig(EST_Xml, xmlCfg);

        ThridServiceConfig platformCfg;
        platformCfg.platform.platform  = cfg.platform;
        platformCfg.platform.enableTrace = false;
        tServiceMgr::getMe().SetServiceConfig(EST_PlatformMgr, platformCfg);
    }

    procMgr.InitEnvironment(cfg.gRPCPort);
    XLOG("[INIT] Environment initialized");

    // =========================================================================
    // Phase 5: Redis 服务发现注册
    //   注册内容: 进程类型 + 进程名称 + 进程ID + 负载信息
    // =========================================================================
    XLOG("[INIT] Phase 5/11: Registering in service discovery");
    if (!procMgr.RegisterInDiscovery()) {
        XWRN("[INIT] Service discovery registration failed — continuing standalone");
    }

    // =========================================================================
    // Phase 6: 启动心跳
    //   5 秒间隔更新: 状态 + 时间戳
    // =========================================================================
    XLOG("[INIT] Phase 6/11: Starting heartbeat (5s interval)");
    procMgr.StartHeartbeat();

    // =========================================================================
    // Phase 7: 加载链接规则
    //   从 Redis 读取进程类型间的链接关系
    // =========================================================================
    XLOG("[INIT] Phase 7/11: Loading link rules");
    auto& linkMgr = CLinkManager::getMe();
    linkMgr.Init();

    // =========================================================================
    // Phase 8: 发现并连接对等进程
    //   依据服务发现中的节点信息建立进程间 gRPC 链接
    //   仅连接状态为 Running 的节点
    //   非 Running 状态的节点: 不进行链接 / 已链接的不分配 Actor
    // =========================================================================
    XLOG("[INIT] Phase 8/11: Discovering and connecting to peers");
    linkMgr.DiscoverAndConnect();

    auto connectedPeers = linkMgr.GetConnectedPeers();
    XLOG("[INIT] Connected to %zu peer(s)", connectedPeers.size());
    for (auto& conn : connectedPeers) {
        XLOG("[INIT]   Peer: %s:%lld @ %s:%u",
             ProcessTypeName(conn.info.type), (int64)conn.info.processId,
             conn.info.host.c_str(), conn.info.grpcPort);
    }

    // =========================================================================
    // Phase 9: Lua VM + Actor 脚本加载
    // =========================================================================
    XLOG("[INIT] Phase 9/11: Initializing LuaVM and loading actors");

    // Actor 依赖注册
    tServiceMgr::getMe().RegisterActorDeps(100,
        { EST_Redis, EST_Log, EST_Xml, EST_PlatformMgr });
    tServiceMgr::getMe().RegisterActorDeps(101,
        { EST_MySql, EST_Redis, EST_Log, EST_Xml, EST_PlatformMgr });
    tServiceMgr::getMe().RegisterActorDeps(201,
        { EST_MySql, EST_Redis, EST_Log, EST_Xml, EST_PlatformMgr });
    tServiceMgr::getMe().RegisterActorDeps(202,
        { EST_MySql, EST_Redis, EST_Log, EST_Xml, EST_PlatformMgr });

    if (!LuaVM::instance()->Init("script")) {
        XERR("[INIT] LuaVM init FAILED");
        return 1;
    }

    int32 actorCount = LuaVM::instance()->LoadAllActors(cfg.platform);
    if (actorCount == 0) {
        XERR("[INIT] No actor init.lua found under script/%s/", cfg.platform.c_str());
        LuaVM::instance()->Release();
        return 1;
    }
    XLOG("[INIT] %d actors loaded from script/%s/", actorCount, cfg.platform.c_str());

    // =========================================================================
    // Phase 10: Actor 安装启动
    // =========================================================================
    XLOG("[INIT] Phase 10/11: Installing actors");
    for (auto type : CActorMgr::instance()->GetHostedTypes()) {
        int32 installed = CActorMgr::instance()->InstallAll(type);
        XLOG("[INIT]   Actor type %d: %d instances installed", type, installed);
    }

    // 标记进程为 Running 状态
    procMgr.SetState(EProcessState_Running);

    // =========================================================================
    // Phase 11: 主循环
    //   定期刷新对等进程状态 + 健康检查
    // =========================================================================
    XLOG("[INIT] ============================================================");
    XLOG("[INIT] RealServer READY — all phases complete");
    XLOG("[INIT] Process: %s id=%lld @ %s:%u",
         ProcessTypeName(selfInfo.type), selfInfo.processId,
         selfInfo.host.c_str(), selfInfo.grpcPort);
    XLOG("[INIT] ============================================================");

    int refreshCounter = 0;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        refreshCounter++;

        // 每 10 秒刷新一次对等进程状态
        if (refreshCounter % 10 == 0) {
            linkMgr.RefreshLoop();
            // 发现新节点并尝试连接
            linkMgr.DiscoverAndConnect();
        }
    }

    // =========================================================================
    // 优雅关闭 (unreachable in current loop)
    // =========================================================================
    procMgr.SetState(EProcessState_Stopping);
    XLOG("[INIT] Shutting down...");
    procMgr.StopHeartbeat();
    discovery.UpdateState(EProcessState_Shutdown);
    discovery.Deregister();

    for (auto type : CActorMgr::instance()->GetHostedTypes()) {
        CActorMgr::instance()->UnInstallAll(type);
    }
    LuaVM::instance()->Release();
    XLOG("[INIT] Server stopped");

    return 0;
}
