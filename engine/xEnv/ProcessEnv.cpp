#include "ProcessEnv.h"

#include "xLog/xLog.h"
#include "xConfig/xServerConfig.h"
#include "xRedis/RedisMgr.h"
#include "xScheduler/xSchedulerMgr.h"
#include "xRpc/CRpcTcpServer.h"
#include "xRpc/CRpcTcpClient.h"
#include "xRpc/ActorRpcHandlers.h"
#include "xActor/ActorMgr.h"
#include "xActor/ActorScheduler.h"
#include "xActor/CActorReportMgr.h"
#include "xNet/CNetGateway.h"
#include "luacore/LuaVM.h"
#include "tService/tServiceMgr.h"
#include "tService/tThridService.h"
#include "xCluster/xProcessManager.h"
#include "xCluster/xServiceDiscovery.h"
#include "xCluster/xLinkManager.h"
#include "xCluster/CActorLoadManager.h"
#include "xTrace/CTraceManager.h"
#include "xGm/GmCommandMgr.h"

#include <chrono>
#include <thread>

// =============================================================================
// OnInit — 进程身份创建 (配置已在 main 中加载, type 已作为启动参数传入)
// =============================================================================
bool CProcessEnv::OnInit()
{
    XLOG("[INIT] ============================================================");
    XLOG("[INIT] RealServer starting (env type=%s)", ProcessTypeName(m_eType));
    XLOG("[INIT] ============================================================");

    auto &cfg = CServerConfig::getMe().GetConfig();
    XLOG("[INIT] Config: platform=%s rpc=%u process_type=%d process_name=%s", cfg.platform.c_str(),
         cfg.rpcPort, cfg.process.type, cfg.process.name.c_str());

    // 创建进程身份: 优先 Redis 生成 ID, 失败则本地回退
    XLOG("[INIT] Creating process identity");
    auto &procMgr = CProcessManager::getMe();

    auto &discovery = CServiceDiscovery::getMe();
    // 连接服务发现 Redis 池 (供集群注册/发现使用, 必须先于 discovery.Init 连接)
    const ThridServiceConfig *discCfg
        = CServerConfig::getMe().FindRedisCfg(cfg.discoveryRedis.poolTitle);
    if (discCfg)
    {
        ThridRedisConfig redisCfg = discCfg->redis;
        redisCfg.title = cfg.discoveryRedis.poolTitle;
        if (!CRedisPoolMgr::getMe().ConnectToRedis(redisCfg))
        {
            XWRN("[INIT] Failed to connect cluster_discovery redis pool");
        }
    }
    discovery.Init(cfg.discoveryRedis.poolTitle);

    EProcessType procType = (EProcessType)cfg.process.type;
    bool procCreated
        = procMgr.CreateProcess(procType, cfg.process.name, cfg.clusterFallback.local.processId);
    if (!procCreated)
    {
        XLOG("[INIT] Using local fallback config for process identity");
        procMgr.CreateProcessFromFallback(cfg.clusterFallback.local);
    }

    auto &selfInfo = procMgr.GetSelfInfo();
    XLOG("[INIT] Process identity: type=%s(%d) id=%lld name=%s", ProcessTypeName(selfInfo.type),
         (int32)selfInfo.type, selfInfo.processId, selfInfo.processName.c_str());

    return true;
}

// =============================================================================
// OnKernelEnable — 按 m_uKernel 启用内核模块 (调度器/gRPC/三方服务)
// =============================================================================
bool CProcessEnv::OnKernelEnable()
{
    XLOG("[INIT] Initializing environment (kernel=0x%X)", m_uKernel);
    auto &cfg = CServerConfig::getMe().GetConfig();
    auto &procMgr = CProcessManager::getMe();
    auto &selfInfo = procMgr.GetSelfInfo(); // 已含分配的 rpcPort / ip / authKey

    // 链路追踪管理模块 (协议延迟采样 + 慢消息上报)
    CTraceManager::instance()->Init(cfg.discoveryRedis.poolTitle, selfInfo);

    // GM 命令模块 (Redis pub/sub 控制面): 框架命令 + Actor 命令转发 + Actor 注册。
    // 须在 OnActorInit (Actor InstallAll) 之前初始化, 否则启动期 Actor 注册会因
    // m_pub 未就绪而被丢弃。
    {
        const ThridServiceConfig *discCfg
            = CServerConfig::getMe().FindRedisCfg(cfg.discoveryRedis.poolTitle);
        if (discCfg)
        {
            CGmCommandMgr::instance()->Init(discCfg->redis, selfInfo.type, selfInfo.processId);
        }
        else
        {
            XWRN("[INIT] GM module skipped — discovery redis config not found");
        }
    }

    // Actor 数据监控上报模块 (订阅 cluster:actor:report:req, 复用共享 RedisCluster)
    CActorReportMgr::instance()->Init(CGmCommandMgr::instance()->GetCli());

    // 调度器 (Kernel_Scheduler) — 所有类型都需要
    xSchedulerMgr::getMe().Init(4 /*协程线程*/, 4 /*同步线程*/);

    // 自建 asio-TCP RPC 服务器 + 客户端 (gRPC 退役)
    RegisterActorRpcHandlers();
    CRpcTcpServer::instance()->Start(selfInfo.rpcPort);
    CRpcTcpClient::instance()->Init();

    // 第三方服务配置 — 按内核模块掩码启用
    if (HasModule(Kernel_Redis) && !cfg.redisConfigs.empty())
    {
        tServiceMgr::getMe().SetServiceConfig(EST_Redis, cfg.redisConfigs[0]);
    }
    if (HasModule(Kernel_MySql) && !cfg.dbConfigs.empty())
    {
        tServiceMgr::getMe().SetServiceConfig(EST_MySql, cfg.dbConfigs[0]);
    }
    {
        ThridServiceConfig logCfg;
        logCfg.log.logPath = "logs/";
        logCfg.log.logLevel = "info";
        tServiceMgr::getMe().SetServiceConfig(EST_Log, logCfg);

        ThridServiceConfig xmlCfg;
        xmlCfg.xml.configPath = "config/server.xml";
        tServiceMgr::getMe().SetServiceConfig(EST_Xml, xmlCfg);

        ThridServiceConfig platformCfg;
        platformCfg.platform.platform = cfg.platform;
        platformCfg.platform.enableTrace = false;
        tServiceMgr::getMe().SetServiceConfig(EST_PlatformMgr, platformCfg);
    }

    procMgr.InitEnvironment(selfInfo.rpcPort);
    XLOG("[INIT] Environment initialized (rpcPort=%u)", selfInfo.rpcPort);
    return true;
}

// =============================================================================
// OnRegister — 服务发现注册 + 心跳
// =============================================================================
bool CProcessEnv::OnRegister()
{
    XLOG("[INIT] Registering in service discovery");
    auto &procMgr = CProcessManager::getMe();

    if (!procMgr.RegisterInDiscovery())
    {
        XWRN("[INIT] Service discovery registration failed — continuing standalone");
        return false;
    }

    XLOG("[INIT] Starting heartbeat (5s interval)");
    procMgr.StartHeartbeat();
    return true;
}

// =============================================================================
// OnActorInit — 链接对等进程 + Lua 层 + Actor 安装与协程调度
// =============================================================================
bool CProcessEnv::OnActorInit()
{
    auto &cfg = CServerConfig::getMe().GetConfig();
    auto &procMgr = CProcessManager::getMe();

    // 链接规则 + 发现并连接对等进程
    XLOG("[INIT] Loading link rules");
    auto &linkMgr = CLinkManager::getMe();
    linkMgr.Init();

    XLOG("[INIT] Discovering and connecting to peers");
    linkMgr.DiscoverAndConnect();

    auto connectedPeers = linkMgr.GetConnectedPeers();
    XLOG("[INIT] Connected to %zu peer(s)", connectedPeers.size());
    for (auto &conn : connectedPeers)
    {
        XLOG("[INIT]   Peer: %s:%lld @ %s:%u", ProcessTypeName(conn.info.type),
             (int64)conn.info.processId, conn.info.host.c_str(), conn.info.rpcPort);
    }

    // Actor 依赖注册 (每个 EActorType 需要哪些三方服务)
    tServiceMgr::getMe().RegisterActorDeps(EActorType_AGate,
                                           {EST_Redis, EST_Log, EST_Xml, EST_PlatformMgr});
    tServiceMgr::getMe().RegisterActorDeps(
        EActorType_ALogin, {EST_MySql, EST_Redis, EST_Log, EST_Xml, EST_PlatformMgr});
    tServiceMgr::getMe().RegisterActorDeps(
        EActorType_AFriend, {EST_MySql, EST_Redis, EST_Log, EST_Xml, EST_PlatformMgr});
    tServiceMgr::getMe().RegisterActorDeps(EActorType_ARoom,
                                           {EST_Redis, EST_Log, EST_Xml, EST_PlatformMgr});
    tServiceMgr::getMe().RegisterActorDeps(EActorType_AChat,
                                           {EST_Redis, EST_Log, EST_Xml, EST_PlatformMgr});
    tServiceMgr::getMe().RegisterActorDeps(EActorType_AConfig,
                                           {EST_Redis, EST_Log, EST_Xml, EST_PlatformMgr});
    tServiceMgr::getMe().RegisterActorDeps(
        EActorType_APlayer, {EST_MySql, EST_Redis, EST_Log, EST_Xml, EST_PlatformMgr});
    tServiceMgr::getMe().RegisterActorDeps(
        EActorType_ABattle, {EST_MySql, EST_Redis, EST_Log, EST_Xml, EST_PlatformMgr});
    tServiceMgr::getMe().RegisterActorDeps(EActorType_ASpectator,
                                           {EST_Redis, EST_Log, EST_Xml, EST_PlatformMgr});

    // 统一 Actor 负载管理器: 从启动进程类型拉取负载类型并注册 (取代 Lua hostType)
    CActorLoadManager::instance()->Init(m_eType);

    // Lua VM + Actor 脚本加载
    XLOG("[INIT] Initializing LuaVM and loading actors");
    if (!LuaVM::instance()->Init("script"))
    {
        XERR("[INIT] LuaVM init FAILED");
        return false;
    }

    int32 actorCount = LuaVM::instance()->LoadAllActors(
        cfg.platform, CServerConfig::getMe().GetHostedActorTypes(m_eType));
    if (actorCount == 0)
    {
        XERR("[INIT] No actor init.lua found under script/%s/", cfg.platform.c_str());
        LuaVM::instance()->Release();
        return false;
    }
    XLOG("[INIT] %d actors loaded from script/%s/", actorCount, cfg.platform.c_str());

    // 安装 Actor
    XLOG("[INIT] Installing actors");
    for (auto type : CActorMgr::instance()->GetHostedTypes())
    {
        int32 installed = CActorMgr::instance()->InstallAll(type);
        XLOG("[INIT]   Actor type %d: %d instances installed", type, installed);
    }

    // 启动 Actor 协程消息调度器，为已安装的 Actor 挂消息循环
    CActorScheduler::instance()->Init(1);
    auto &schedIoc = CActorScheduler::instance()->GetContext();
    for (auto type : CActorMgr::instance()->GetHostedTypes())
    {
        for (auto *pActor : CActorMgr::instance()->GetByType(type))
        {
            pActor->BindResumeContext(schedIoc);
            CActorScheduler::instance()->Spawn(pActor);
        }
    }

    return true;
}

// =============================================================================
// OnRunning — 网络接入 (Gate) + 标记 Running + 主循环
// =============================================================================
bool CProcessEnv::OnRunning()
{
    auto &cfg = CServerConfig::getMe().GetConfig();
    auto &procMgr = CProcessManager::getMe();
    auto &linkMgr = CLinkManager::getMe();

    // 网络接入层 — 仅 Gate 启用 (Kernel_Net)
    if (HasModule(Kernel_Net))
    {
        XLOG("[INIT] Starting network gateway (port=%u)", cfg.process.listenPort);
        CNetGateway::instance()->Start(cfg.process.listenPort);
    }

    // 标记进程为 Running 状态
    procMgr.SetState(EProcessState_Running);

    XLOG("[INIT] ============================================================");
    XLOG("[INIT] RealServer READY — all phases complete");
    XLOG("[INIT] Process: %s id=%lld @ %s:%u", ProcessTypeName(procMgr.GetSelfInfo().type),
         procMgr.GetSelfInfo().processId, procMgr.GetSelfInfo().host.c_str(),
         procMgr.GetSelfInfo().rpcPort);
    XLOG("[INIT] ============================================================");

    int32 refreshCounter = 0;
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        refreshCounter++;

        // 每 10 秒刷新一次对等进程状态 + 链路追踪阈值
        if (refreshCounter % 10 == 0)
        {
            linkMgr.RefreshLoop();
            linkMgr.DiscoverAndConnect();
            CTraceManager::instance()->RefreshThreshold();
        }
    }

    return true; // unreachable
}

// =============================================================================
// OnStop — 优雅关闭 (主循环为死循环, 正常不可达)
// =============================================================================
void CProcessEnv::OnStop()
{
    auto &procMgr = CProcessManager::getMe();
    auto &discovery = CServiceDiscovery::getMe();

    procMgr.SetState(EProcessState_Stopping);
    XLOG("[INIT] Shutting down...");
    procMgr.StopHeartbeat();
    discovery.UpdateState(EProcessState_Shutdown);
    discovery.Deregister();

    for (auto type : CActorMgr::instance()->GetHostedTypes())
    {
        CActorMgr::instance()->UnInstallAll(type);
    }
    CGmCommandMgr::instance()->Shutdown();
    LuaVM::instance()->Release();
    XLOG("[INIT] Server stopped");
}
