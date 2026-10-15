#include "xProcessManager.h"
#include "xServiceDiscovery.h"
#include "xLocalInfo.h"
#include "xConfig/xServerConfig.h"
#include "xActor/ActorMgr.h"
#include "xActor/CActorReportMgr.h"
#include "CActorLoadManager.h"
#include "CPlayerActorIndex.h"
#include "xActorId.h"
#include "xGm/GmCommandMgr.h"
#include "xRedis/RedisCli.h"
#include "xBase/xSysInfo.h"
#include "xLog/xLog.h"
#include <chrono>
#include <ctime>

// =============================================================================
// CreateProcess — 进程节点初始化全流程
//
//   a. Redis INCR 生成全局唯一进程 ID (不可用则回退 fallbackId)
//   b. CLocalInfo::Init 加载本机 local.info
//   c. AllocatePort 分配本类型可用 RPC 端口 (基础端口 + 递增)
//   d. 探测本机实际 IPv4
//   e. 由进程 ID + 密钥派生验证 Key
//   f. 写入 local.info 本机节点注册表
//   g. 向 Redis 确认节点详情 (IP/Port/AuthKey), 失败退化为 standalone
// =============================================================================
bool CProcessManager::CreateProcess(EProcessType type, const std::string &name, int64 fallbackId)
{
    XLOG("[ProcessManager] Creating process: type=%d name=%s", (int32)type, name.c_str());

    auto &cfg = CServerConfig::getMe().GetConfig();
    auto &disc = CServiceDiscovery::getMe();
    auto &local = CLocalInfo::getMe();

    m_stSelfInfo.type = type;
    m_stSelfInfo.processName = name;
    m_stSelfInfo.host = cfg.process.host.empty() ? "127.0.0.1" : cfg.process.host;
    m_stSelfInfo.listenPort = cfg.process.listenPort;
    m_stSelfInfo.machineId = cfg.process.machineId;
    // 二进制版本: --version 传入的「时间戳.md5」优先, 否则回退编译期 git 版本
    m_stSelfInfo.version = cfg.process.version.empty() ? kProcessVersion : cfg.process.version;
    m_stSelfInfo.state = EProcessState_Starting;

    // a. Redis 自增生成全局唯一进程 ID (不可用则本地回退)
    int64 id = disc.IsRedisAvailable() ? disc.GenerateProcessId(type) : -1;
    if (id <= 0)
    {
        XWRN("[ProcessManager] Redis ID unavailable (id=%lld) — fallback to %lld", id, fallbackId);
        id = fallbackId;
    }
    else
    {
        XLOG("[ProcessManager] Redis generated process ID: %lld", id);
    }
    m_stSelfInfo.processId = id;
    m_stSelfInfo.osPid = (int64)xActorId::GetOsPid(); // 供编排脚本把 OS pid 映射到 processId

    // 注册即标记心跳为「现在」, 避免刚注册、尚未发出首次心跳(5s)时被对端
    // 心跳过滤误判为下线 (僵尸进程过滤见 DiscoverPeers)。
    m_stSelfInfo.lastHeartbeat = (int64)time(nullptr);

    // b. 加载本机节点注册表 local.info
    local.Init(".");

    // c. 分配本类型可用 RPC 端口
    m_stSelfInfo.rpcPort = local.AllocatePort(type);
    XLOG("[ProcessManager] Allocated RPC port %u for type %s", m_stSelfInfo.rpcPort,
         ProcessTypeName(type));

    // d. 本机实际 IPv4
    m_stSelfInfo.ip = CLocalInfo::GetLocalIp();
    XLOG("[ProcessManager] Local IPv4 = %s", m_stSelfInfo.ip.c_str());

    // e. 验证 Key (进程 ID + 密钥派生)
    std::string secret = cfg.authSecret.empty() ? "RealServerAuthSecret" : cfg.authSecret;
    m_stSelfInfo.authKey = GenerateAuthKey(m_stSelfInfo.processId, secret);

    // f. 写入 local.info 本机节点注册表
    if (!local.Register(m_stSelfInfo))
    {
        XERR("[ProcessManager] Failed to write local.info");
    }
    else
    {
        XLOG("[ProcessManager] Registered in local.info: id=%lld ip=%s rpc=%u listen=%u auth=%s",
             m_stSelfInfo.processId, m_stSelfInfo.ip.c_str(), m_stSelfInfo.rpcPort,
             m_stSelfInfo.listenPort, m_stSelfInfo.authKey.c_str());
    }

    // g. 向 Redis 确认节点详情 (IP/Port/AuthKey), 失败不致命 → 退化为 standalone
    if (disc.IsRedisAvailable())
    {
        if (disc.ConfirmNodeDetail(m_stSelfInfo))
        {
            XCLOG("Node detail confirmed in Redis: %s", m_stSelfInfo.RedisKey().c_str());
        }
        else
        {
            XWRN("[ProcessManager] Redis node detail confirmation failed — running standalone");
        }
    }

    XLOG("[ProcessManager] Process created: type=%s(%d) id=%lld name=%s ip=%s rpc=%u",
         ProcessTypeName(m_stSelfInfo.type), (int32)m_stSelfInfo.type, m_stSelfInfo.processId,
         m_stSelfInfo.processName.c_str(), m_stSelfInfo.ip.c_str(), m_stSelfInfo.rpcPort);
    return true;
}

bool CProcessManager::CreateProcessFromFallback(const ClusterLocalConfig &localCfg)
{
    XLOG("[ProcessManager] Creating process from fallback config");

    auto &cfg = CServerConfig::getMe().GetConfig();
    auto &local = CLocalInfo::getMe();

    m_stSelfInfo.type = localCfg.type;
    m_stSelfInfo.processId = localCfg.processId;
    m_stSelfInfo.osPid = (int64)xActorId::GetOsPid();
    m_stSelfInfo.processName = localCfg.name;
    m_stSelfInfo.state = EProcessState_Starting;
    m_stSelfInfo.host = cfg.process.host.empty() ? "127.0.0.1" : cfg.process.host;
    m_stSelfInfo.listenPort = cfg.process.listenPort;
    m_stSelfInfo.machineId = cfg.process.machineId;
    m_stSelfInfo.version = cfg.process.version.empty() ? kProcessVersion : cfg.process.version;
    m_stSelfInfo.lastHeartbeat = (int64)time(nullptr);

    local.Init(".");
    m_stSelfInfo.rpcPort = local.AllocatePort(m_stSelfInfo.type);
    m_stSelfInfo.ip = CLocalInfo::GetLocalIp();
    std::string secret = cfg.authSecret.empty() ? "RealServerAuthSecret" : cfg.authSecret;
    m_stSelfInfo.authKey = GenerateAuthKey(m_stSelfInfo.processId, secret);
    local.Register(m_stSelfInfo);

    XLOG("[ProcessManager] Process created (fallback): type=%d id=%lld name=%s ip=%s rpc=%u",
         (int32)localCfg.type, localCfg.processId, localCfg.name.c_str(), m_stSelfInfo.ip.c_str(),
         m_stSelfInfo.rpcPort);
    return true;
}

// =============================================================================
// InitEnvironment
// =============================================================================
bool CProcessManager::InitEnvironment(uint16 rpcPort)
{
    XLOG("[ProcessManager] Initializing environment...");

    // 注意: 日志系统已在 main.cpp Phase 1 中初始化
    // 调度器在 main.cpp Phase 2 中初始化
    // asio-TCP RPC 服务端/客户端在 ProcessEnv::OnKernelEnable 中启动 (gRPC 已退役)
    // 这里只做本模块负责的部分 (标记状态)

    // 标记状态
    m_stSelfInfo.state = EProcessState_Init;
    XLOG("[ProcessManager] Environment initialized for process %s:%lld",
         ProcessTypeName(m_stSelfInfo.type), m_stSelfInfo.processId);
    return true;
}

// =============================================================================
// RegisterInDiscovery
// =============================================================================
bool CProcessManager::RegisterInDiscovery()
{
    auto &disc = CServiceDiscovery::getMe();
    if (!disc.Register(m_stSelfInfo))
    {
        XERR("[ProcessManager] Failed to register in service discovery");
        return false;
    }
    XCLOG("Registered in service discovery");
    return true;
}

// =============================================================================
// Heartbeat
// =============================================================================
void CProcessManager::StartHeartbeat()
{
    if (m_bHeartbeatRunning)
    {
        return;
    }
    m_bStopHeartbeat = false;
    m_bHeartbeatRunning = true;
    m_heartbeatThread = std::thread(&CProcessManager::HeartbeatLoop, this);
    XLOG("[ProcessManager] Heartbeat started (heavy=5s, actor-report=1s)");
}

void CProcessManager::StopHeartbeat()
{
    m_bStopHeartbeat = true;
    if (m_heartbeatThread.joinable())
    {
        m_heartbeatThread.join();
    }
    m_bHeartbeatRunning = false;
    XLOG("[ProcessManager] Heartbeat stopped");
}

void CProcessManager::HeartbeatLoop()
{
    // 1s 细粒度 tick: Actor 监控上报 (Sweep) 每秒一次; 心跳/负载/资源/状态 flush 仍按 5s
    // 节拍 (tick%5==0), 避免把服务发现心跳与 Redis TTL 续期频率也抬到 1s。
    int32 tick = 0;
    while (!m_bStopHeartbeat)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        if (m_bStopHeartbeat)
        {
            break;
        }
        ++tick;
        const bool bHeavy = (tick % 5 == 0);

        auto &disc = CServiceDiscovery::getMe();
        if (disc.IsRedisAvailable())
        {
            if (bHeavy)
            {
                // 采样本进程资源占用 (CPU% / RSS), 供资源感知负载均衡与 WebPlatform 展示
                int32 cpuPercent = xSysInfo::GetProcessCpuPercent();
                int64 memRssMb = xSysInfo::GetProcessMemRssMb();
                int64 memTotal = xSysInfo::GetMachineTotalMemMb();
                int32 memPercent = memTotal > 0 ? (int32)((memRssMb * 100) / memTotal) : 0;

                disc.UpdateState(m_stSelfInfo.state);
                disc.Heartbeat();
                disc.UpdateLoad(
                    // 从统一 Actor 负载管理器获取真实负载 (本进程承载的 actor 实例数)
                    CActorLoadManager::instance()->GetLocalLoad());
                disc.UpdateResource(cpuPercent, memRssMb, memPercent);

                // P9: 版本 + 内容灰度上报 (读 cluster:update:content:config, 灰度桶取模)
                std::string contentVersion = m_stSelfInfo.version;
                std::string targetVer = disc.GetContentUpdateConfig("version");
                std::string grayStr = disc.GetContentUpdateConfig("gray_percent");
                std::string updStatus = disc.GetContentUpdateConfig("status");
                if (!targetVer.empty() && targetVer != m_stSelfInfo.version
                    && (updStatus.empty() || updStatus == "rolling"))
                {
                    int32 grayPct = grayStr.empty() ? 0 : atoi(grayStr.c_str());
                    if (grayPct > 0 && (m_stSelfInfo.processId % 100) < grayPct)
                    {
                        contentVersion = targetVer; // 命中灰度桶 → 采用新内容版本
                    }
                }
                disc.UpdateVersion(m_stSelfInfo.version, contentVersion);

                // P7: Actor 帧级脏状态 flush (写 actor:state:{type}:{id}) + 各项 Redis 键 TTL 续期
                if (CRedisCli *stateCli = CGmCommandMgr::instance()->GetCli())
                {
                    for (EActorType t : CActorMgr::instance()->GetHostedTypes())
                    {
                        for (CActor *a : CActorMgr::instance()->GetByType(t))
                        {
                            a->FlushState(stateCli);
                            // 恢复快照 TTL 续期 (24h, 宕机后动态恢复窗口)
                            a->RefreshStateTtl(stateCli);
                            // 注册键 TTL 续期 (cluster:actor:*, 30s, 僵尸 Actor 防护)
                            CGmCommandMgr::instance()->RefreshActorRegistration(t, a->GetActorID());
                            // Player 路由索引 TTL 续期 (player:{id}, 30s)
                            if (t == EActorType_APlayer && a->GetActorID() != 0)
                            {
                                CPlayerActorIndex::instance()->RefreshTtl((int64)a->GetActorID());
                            }
                        }
                    }
                }
            }

            // Actor 数据监控上报扫荡 (订阅期内写 cluster:actor:report:{type}:{id}) —— 每秒一次
            CActorReportMgr::instance()->Sweep();
        }
    }
}

// =============================================================================
// State
// =============================================================================
bool CProcessManager::SetState(EProcessState state)
{
    m_stSelfInfo.state = state;
    auto &disc = CServiceDiscovery::getMe();
    if (disc.IsRedisAvailable())
    {
        disc.UpdateState(state);
    }
    XCLOG("State changed: %s → %s", ProcessStateName(m_stSelfInfo.state), ProcessStateName(state));
    return true;
}
