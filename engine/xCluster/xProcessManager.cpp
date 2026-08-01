#include "xProcessManager.h"
#include "xServiceDiscovery.h"
#include "xConfig/xServerConfig.h"
#include "xRpc/xRpc.h"
#include "xActor/ActorMgrService.h"
#include "xLog/xLog.h"
#include <chrono>

// =============================================================================
// CreateProcess
// =============================================================================
bool CProcessManager::CreateProcess(EProcessType type, const std::string& name, int64 fallbackId) {
    XLOG("[ProcessManager] Creating process: type=%d name=%s", (int32)type, name.c_str());

    m_stSelfInfo.type        = type;
    m_stSelfInfo.processName = name;
    m_stSelfInfo.state       = EProcessState_Starting;

    // Step 1: 尝试从 Redis 获取配置 + 生成唯一 ID
    auto& disc = CServiceDiscovery::getMe();
    if (disc.IsRedisAvailable()) {
        int64 id = disc.GenerateProcessId(type);
        if (id > 0) {
            m_stSelfInfo.processId = id;
            XLOG("[ProcessManager] Redis generated process ID: %lld", id);
        } else {
            XWRN("[ProcessManager] Redis ID generation failed — using fallback ID %lld", fallbackId);
            m_stSelfInfo.processId = fallbackId;
        }
    } else {
        XLOG("[ProcessManager] Redis unavailable — using fallback ID %lld", fallbackId);
        m_stSelfInfo.processId = fallbackId;
    }

    // 从 server.xml 读取 host/port
    auto& cfg = CServerConfig::getMe().GetConfig();
    m_stSelfInfo.host     = cfg.process.host.empty() ? "127.0.0.1" : cfg.process.host;
    m_stSelfInfo.grpcPort = cfg.gRPCPort;

    XLOG("[ProcessManager] Process created: type=%s(%d) id=%lld name=%s host=%s:%u",
         ProcessTypeName(m_stSelfInfo.type), (int32)m_stSelfInfo.type,
         m_stSelfInfo.processId, m_stSelfInfo.processName.c_str(),
         m_stSelfInfo.host.c_str(), m_stSelfInfo.grpcPort);
    return true;
}

bool CProcessManager::CreateProcessFromFallback(const ClusterLocalConfig& localCfg) {
    XLOG("[ProcessManager] Creating process from fallback config");

    m_stSelfInfo.type        = (EProcessType)localCfg.type;
    m_stSelfInfo.processId   = localCfg.processId;
    m_stSelfInfo.processName = localCfg.name;
    m_stSelfInfo.state       = EProcessState_Starting;

    auto& cfg = CServerConfig::getMe().GetConfig();
    m_stSelfInfo.host     = cfg.process.host.empty() ? "127.0.0.1" : cfg.process.host;
    m_stSelfInfo.grpcPort = cfg.gRPCPort;

    XLOG("[ProcessManager] Process created (fallback): type=%d id=%lld name=%s",
         localCfg.type, localCfg.processId, localCfg.name.c_str());
    return true;
}

// =============================================================================
// InitEnvironment
// =============================================================================
bool CProcessManager::InitEnvironment(uint16 grpcPort) {
    XLOG("[ProcessManager] Initializing environment...");

    // 注意: 日志系统已在 main.cpp Phase 1 中初始化
    // 调度器在 main.cpp Phase 2 中初始化
    // 这里只做本模块负责的部分

    // 标记状态
    m_stSelfInfo.state = EProcessState_Init;
    XLOG("[ProcessManager] Environment initialized for process %s:%lld",
         ProcessTypeName(m_stSelfInfo.type), m_stSelfInfo.processId);
    return true;
}

// =============================================================================
// RegisterInDiscovery
// =============================================================================
bool CProcessManager::RegisterInDiscovery() {
    auto& disc = CServiceDiscovery::getMe();
    if (!disc.Register(m_stSelfInfo)) {
        XERR("[ProcessManager] Failed to register in service discovery");
        return false;
    }
    XCLOG("Registered in service discovery");
    return true;
}

// =============================================================================
// Heartbeat
// =============================================================================
void CProcessManager::StartHeartbeat() {
    if (m_bHeartbeatRunning) return;
    m_bStopHeartbeat = false;
    m_bHeartbeatRunning = true;
    m_heartbeatThread = std::thread(&CProcessManager::HeartbeatLoop, this);
    XLOG("[ProcessManager] Heartbeat started (interval=5s)");
}

void CProcessManager::StopHeartbeat() {
    m_bStopHeartbeat = true;
    if (m_heartbeatThread.joinable()) {
        m_heartbeatThread.join();
    }
    m_bHeartbeatRunning = false;
    XLOG("[ProcessManager] Heartbeat stopped");
}

void CProcessManager::HeartbeatLoop() {
    while (!m_bStopHeartbeat) {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        if (m_bStopHeartbeat) break;

        auto& disc = CServiceDiscovery::getMe();
        if (disc.IsRedisAvailable()) {
            disc.UpdateState(m_stSelfInfo.state);
            disc.Heartbeat();
            disc.UpdateLoad(
                // 从 CActorMgr 获取负载
                0  // TODO: CActorMgr::instance()->GetInstanceCount()
            );
        }
    }
}

// =============================================================================
// State
// =============================================================================
bool CProcessManager::SetState(EProcessState state) {
    m_stSelfInfo.state = state;
    auto& disc = CServiceDiscovery::getMe();
    if (disc.IsRedisAvailable()) {
        disc.UpdateState(state);
    }
    XCLOG("State changed: %s → %s",
         ProcessStateName(m_stSelfInfo.state), ProcessStateName(state));
    return true;
}
