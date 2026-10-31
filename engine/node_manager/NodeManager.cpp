/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: NodeManager.cpp
      Purpose:   物理机守护进程核心实现

      实现 CNodeManager 的全部方法
      包括子进程拉起 StartChild、崩溃退避重启 ReapExitedChildren、期望数收敛 ReconcileProcesses、Leader 锁自动扩缩容 AutoScale 与进程级灰度滚动重启 ProcessUpdateGray
  *****************************************************************/

#include "NodeManager.h"

#include "xConfig/xServerConfig.h"
#include "xRedis/RedisMgr.h"
#include "xCluster/xProcessDefine.h"
#include "xBase/xSysInfo.h"
#include "xLog/xLog.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <thread>
#include <utility>

#ifndef _WINDOWS
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#endif

// =============================================================================
// 常量
// =============================================================================
namespace
{
const int32 kHeartbeatInterval = 5;  // 机器心跳周期 (秒)
const int32 kReconcileInterval = 2;  // 期望数收敛周期 (秒)
const int32 kLaunchIntervalSec = 2;  // 连续拉起子进程间隔 (秒), 等前一个完成 RPC 端口分配避免竞争
const int32 kAutoScaleInterval = 10; // 自动扩缩容控制器周期 (秒, Leader 专用)
const int32 kMaxRestartCount = 5;    // 连续崩溃上限, 超此标记 faulted 停止拉起
const int32 kMaxBackoffSec = 60;     // 退避重启封顶 (秒)
const int32 kGracefulStopTimeoutSec = 20; // 优雅停止兜底硬杀时限 (秒)
const char *kNodeAllKey = "cluster:node:all";
const char *kAlertKey = "cluster:alert";
const char *kCrashLogKey = "cluster:crash:log";
const char *kControllerLockKey = "cluster:lock:controller";
const int32 kControllerLockTtl = 30; // Leader 锁 TTL (秒), 心跳续期
const int32 kCrashLogMax = 2000;
const int32 kStaleProcessSec = 15; // 进程心跳超时 (采集资源时过滤僵尸)
const char *kMachineCacheFile = "node_manager.machine";

const EProcessType kAllProcessTypes[]
    = {EProcessType_PGate, EProcessType_PLogin, EProcessType_PGlobal, EProcessType_PGame,
       EProcessType_PBattle};

std::string NodeKey(uint32 machineId)
{
    return "cluster:node:" + std::to_string(machineId);
}
std::string DesiredKey(EProcessType t)
{
    return "cluster:desired:" + std::to_string((int32)t);
}

bool PathExists(const std::string &path)
{
#ifdef _WINDOWS
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    return access(path.c_str(), F_OK) == 0;
#endif
}

// 客户端监听端口基础值 — 仅 Gate/Battle 有客户端直连网关 (Kernel_Net), 需同机多实例互斥。
// 与 RPC 端口 (50551+) 分开: Gate 10080-10099 / Battle 10100-10119 (各预留 20 端口)。
// Gate-1 保持 10080 与 TestClientGo 默认网关端口一致。
uint16 ClientBasePort(EProcessType type)
{
    switch (type)
    {
    case EProcessType_PGate:
        return 10080;
    case EProcessType_PBattle:
        return 10100;
    default:
        return 0; // 无客户端监听端口 (Login/Game/Global)
    }
}

// 按实例序号分配客户端监听端口 (name 形如 "Gate-3" → 10080 + (3-1) = 10082)。
uint16 ResolveClientPort(EProcessType type, const std::string &name)
{
    uint16 base = ClientBasePort(type);
    if (base == 0)
    {
        return 0;
    }
    size_t dash = name.rfind('-');
    int32 seq = (dash != std::string::npos) ? atoi(name.c_str() + dash + 1) : 1;
    if (seq < 1)
    {
        seq = 1;
    }
    return (uint16)(base + seq - 1);
}
} // namespace

// =============================================================================
// Init — 解析参数 + 加载配置 + 连 Redis + 机器注册
// =============================================================================
bool CNodeManager::Init(int32 argc, char **argv)
{
    if (!ParseArgs(argc, argv))
    {
        XERR("[NodeManager] argument parse failed");
        return false;
    }
    ResolveRunRoot(); // 先定位仓库根并修正 cwd, 再初始化日志/配置 (相对路径依赖)
    xLog::get_mutable_instance().Init();

    if (!LoadConfig())
    {
        XERR("[NodeManager] failed to load config %s", m_configPath.c_str());
        return false;
    }

    m_machineId = ResolveMachineId();
    if (m_machineId == 0)
    {
        XERR("[NodeManager] no machine id — pass --machine-id=<id>");
        return false;
    }
    m_hostname = GetHostname();

    // 持久化 machine id, 重启复用避免 ID 漂移
    {
        std::ofstream fout(kMachineCacheFile, std::ios::trunc);
        if (fout.is_open())
        {
            fout << m_machineId << '\n';
        }
    }

    RegisterMachine();
    m_running = true;
    XLOG("[NodeManager] initialized: machine=%u host=%s ip=%s redis_title=%s", m_machineId,
         m_hostname.c_str(), m_ip.c_str(), m_redisTitle.c_str());
    return true;
}

void CNodeManager::Shutdown()
{
    m_running = false;
    // 停止所有子进程 (优雅: 先 PUBLISH shutdown 让进程走 OnStop 落盘退出, 超时由 Reap 硬杀兜底)
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto &c : m_children)
    {
        if (c.graceful)
        {
            continue;
        }
        c.graceful = true;
        RequestChildStop(c);
    }
    XLOG("[NodeManager] shutdown requested");
}

// =============================================================================
// Run — 主循环 (死循环): 崩溃拉起 + 期望数收敛 + 机器心跳
// =============================================================================
void CNodeManager::Run()
{
    XLOG("[NodeManager] running (reconcile=%ds heartbeat=%ds)", kReconcileInterval,
         kHeartbeatInterval);
    while (m_running)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        int64 now = (int64)time(nullptr);

        // 1) 崩溃检测 + 退避重启 (每秒)
        ReapExitedChildren();

        // 2) 期望数收敛 (每 2s)
        if (now - m_lastReconcile >= kReconcileInterval)
        {
            m_lastReconcile = now;
            ReconcileProcesses();
        }

        // 3) 机器心跳 + 容量上报 (每 5s)
        if (now - m_lastHeartbeat >= kHeartbeatInterval)
        {
            m_lastHeartbeat = now;
            HeartbeatMachine();
        }

        // 4) 自动扩缩容 + 进程灰度更新 (每 10s, 仅集群 Leader 执行; 非 Leader 静默)
        if (now - m_lastAutoScale >= kAutoScaleInterval)
        {
            m_lastAutoScale = now;
            if (TryAcquireControllerLock())
            {
                AutoScale();
                ProcessUpdateGray();
            }
        }
    }
}

// =============================================================================
// 参数解析
// =============================================================================
bool CNodeManager::ParseArgs(int32 argc, char **argv)
{
    for (int32 i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg.rfind("--machine-id=", 0) == 0)
        {
            m_machineId = (uint32)strtoul(arg.c_str() + 13, nullptr, 10);
        }
        else if (arg.rfind("--platform=", 0) == 0)
        {
            m_platform = arg.substr(11);
        }
        else if (arg.rfind("--config=", 0) == 0)
        {
            m_configPath = arg.substr(9);
        }
        else if (arg.rfind("--ip=", 0) == 0)
        {
            m_ip = arg.substr(5);
        }
    }
    return true;
}

// =============================================================================
// ResolveRunRoot — 定位仓库根目录 (含 config/ 与 script/)
//
// NodeManager 与其拉起的 RealServer 子进程都用相对路径加载 config/server.xml、
// script/、logs/。双击 exe 时工作目录是 exe 所在目录 (build_msvc/engine/Debug),
// 相对路径会全部失效。这里: 若当前 cwd 已含 config/server.xml 则沿用;
// 否则从 exe 目录逐级向上找「含 config/server.xml + script/」的仓库根并切换过去。
// =============================================================================
void CNodeManager::ResolveRunRoot()
{
    if (PathExists(m_configPath) || PathExists("config/server.xml"))
    {
        return; // cwd 已正确 (从仓库根或含 config 的目录启动)
    }
#ifdef _WINDOWS
    char buf[MAX_PATH] = {0};
    if (GetModuleFileNameA(nullptr, buf, MAX_PATH) == 0)
    {
        return;
    }
    std::string dir = buf;
    size_t slash = dir.find_last_of("\\/");
    if (slash == std::string::npos)
    {
        return;
    }
    dir.resize(slash); // 去掉文件名 → exe 所在目录

    while (!dir.empty())
    {
        if (PathExists(dir + "/config/server.xml") && PathExists(dir + "/script"))
        {
            m_runRoot = dir;
            SetCurrentDirectoryA(dir.c_str());
            return; // 注意: 此时 xLog 尚未 Init, 不能在这里打日志
        }
        size_t p = dir.find_last_of("\\/");
        if (p == std::string::npos)
        {
            break;
        }
        dir.resize(p);
    }
#else
    // Linux 约定从仓库根启动 (见 CLAUDE.md), 无法定位时沿用 cwd
    (void)0;
#endif
}

// =============================================================================
// LoadConfig — 读 server.xml 拿 Redis seed + auth, 连共享 RedisCluster
// =============================================================================
bool CNodeManager::LoadConfig()
{
    if (!CServerConfig::getMe().Load(m_configPath))
    {
        XWRN("[NodeManager] server.xml parse failed — using defaults");
    }

    const ThridServiceConfig *discCfg = CServerConfig::getMe().FindRedisCfg(m_redisTitle);
    if (!discCfg)
    {
        XERR("[NodeManager] redis pool '%s' not found in config", m_redisTitle.c_str());
        return false;
    }

    ThridRedisConfig redisCfg = discCfg->redis;
    redisCfg.title = m_redisTitle;
    if (!CRedisPoolMgr::getMe().ConnectToRedis(redisCfg))
    {
        XERR("[NodeManager] connect to redis cluster failed @ %s:%d", redisCfg.host.c_str(),
             redisCfg.port);
        return false;
    }
    m_cli = CRedisPoolMgr::getMe().GetRedisCli(m_redisTitle);
    return m_cli != nullptr;
}

// =============================================================================
// ResolveMachineId — --machine-id 优先, 否则本机文件缓存
// =============================================================================
uint32 CNodeManager::ResolveMachineId()
{
    if (m_machineId != 0)
    {
        return m_machineId;
    }
    std::ifstream fin(kMachineCacheFile);
    if (fin.is_open())
    {
        uint32 id = 0;
        fin >> id;
        if (id > 0)
        {
            XLOG("[NodeManager] reuse cached machine id=%u from %s", id, kMachineCacheFile);
            return id;
        }
    }
    return 0;
}

std::string CNodeManager::GetHostname()
{
#ifdef _WINDOWS
    char buf[256] = {0};
    DWORD sz = sizeof(buf);
    if (GetComputerNameA(buf, &sz))
    {
        return buf;
    }
    return "windows-host";
#else
    char buf[256] = {0};
    if (gethostname(buf, sizeof(buf)) == 0)
    {
        return buf;
    }
    return "linux-host";
#endif
}

// =============================================================================
// 机器注册 / 心跳
// =============================================================================
void CNodeManager::RegisterMachine()
{
    if (!m_cli)
    {
        return;
    }
    std::string key = NodeKey(m_machineId);

    WriteHash(key, "machine_id", std::to_string(m_machineId));
    WriteHash(key, "hostname", m_hostname);
    WriteHash(key, "ip", m_ip);
    WriteHash(key, "cpu_cores", std::to_string(xSysInfo::GetMachineCpuCores()));
    WriteHash(key, "mem_total_mb", std::to_string(xSysInfo::GetMachineTotalMemMb()));
#ifdef _WINDOWS
    WriteHash(key, "os", "windows");
#else
    WriteHash(key, "os", "linux");
#endif
    WriteHash(key, "state", "running");
    WriteHash(key, "heartbeat", std::to_string((int64)time(nullptr)));

    m_cli->SAdd(kNodeAllKey, std::to_string(m_machineId));
    XLOG("[NodeManager] machine registered: %s", key.c_str());
}

void CNodeManager::HeartbeatMachine()
{
    if (!m_cli)
    {
        return;
    }
    std::string key = NodeKey(m_machineId);
    WriteHash(key, "heartbeat", std::to_string((int64)time(nullptr)));
    WriteHash(key, "cpu_cores", std::to_string(xSysInfo::GetMachineCpuCores()));
    WriteHash(key, "mem_total_mb", std::to_string(xSysInfo::GetMachineTotalMemMb()));
}

// =============================================================================
// 期望数收敛 (手动优先: 读 cluster:desired:{type}.count)
// =============================================================================
void CNodeManager::ReconcileProcesses()
{
    if (!m_cli)
    {
        return;
    }

    for (EProcessType type : kAllProcessTypes)
    {
        std::string countStr = ReadHash(DesiredKey(type), "count");
        int32 desired = countStr.empty() ? 0 : atoi(countStr.c_str());
        if (desired < 0)
        {
            desired = 0;
        }

        int32 alive = CountAlive(type);
        if (alive == desired)
        {
            continue;
        }

        if (alive < desired)
        {
            // 扩容
            for (int32 i = alive; i < desired; ++i)
            {
                if (!CapacityOk())
                {
                    XWRN("[NodeManager] capacity cap reached — skip launch type=%s",
                         ProcessTypeName(type));
                    PushAlert(std::string("capacity cap reached, skip launch ")
                              + ProcessTypeName(type));
                    break;
                }
                std::string name = NextName(type);
                if (LaunchProcess(type, name))
                {
                    // 成功; 计数由 m_children 反映。等前一个子进程完成 RPC 端口分配
                    // (local.info) 再起下一个, 避免同类型端口竞争 (与手动部署 2s stagger 一致)。
                    std::this_thread::sleep_for(std::chrono::seconds(kLaunchIntervalSec));
                }
                else
                {
                    XERR("[NodeManager] launch failed type=%s name=%s", ProcessTypeName(type),
                         name.c_str());
                    break;
                }
            }
        }
        else
        {
            // 缩容: 标记多余子进程为 graceful 并终止
            int32 excess = alive - desired;
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto &c : m_children)
            {
                if (excess <= 0)
                {
                    break;
                }
                if (c.type != type || c.graceful)
                {
                    continue;
                }
                c.graceful = true;
                excess--;
                RequestChildStop(c);
                XLOG("[NodeManager] scaling down: stop %s type=%s", c.name.c_str(),
                     ProcessTypeName(type));
            }
        }
    }
}

int32 CNodeManager::CountAlive(EProcessType type) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    int32 n = 0;
    for (auto &c : m_children)
    {
        if (c.type == type && !c.graceful)
        {
            ++n;
        }
    }
    return n;
}

bool CNodeManager::CapacityOk()
{
    // 本机进程总量上限 (可被 WebPlatform 覆盖), 缺省按 cpu_cores 折算
    int32 cap = 0;
    std::string capStr
        = ReadHash("cluster:config:node:" + std::to_string(m_machineId), "capacity_cap");
    if (!capStr.empty())
    {
        cap = atoi(capStr.c_str());
    }
    if (cap <= 0)
    {
        int32 cores = xSysInfo::GetMachineCpuCores();
        cap = (cores > 0 ? cores : 1) * 4;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    int32 active = 0;
    for (auto &c : m_children)
    {
        if (!c.graceful)
        {
            ++active;
        }
    }
    return active < cap;
}

std::string CNodeManager::NextName(EProcessType type) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    int32 maxSeq = 0;
    for (auto &c : m_children)
    {
        if (c.type != type)
        {
            continue;
        }
        // name 形如 "Gate-3" / "Login-1"
        size_t dash = c.name.rfind('-');
        if (dash != std::string::npos)
        {
            int32 seq = atoi(c.name.c_str() + dash + 1);
            if (seq > maxSeq)
            {
                maxSeq = seq;
            }
        }
    }
    return std::string(ProcessTypeName(type)) + "-" + std::to_string(maxSeq + 1);
}

// =============================================================================
// 子进程起停
// =============================================================================
bool CNodeManager::StartChild(ChildInfo &c)
{
    // Gate/Battle 有客户端直连网关, 需同机互斥的监听端口 (按实例序号分配);
    // 其余类型无客户端监听, 不传 --port (沿用 server.xml 默认, 不会绑网关)。
    uint16 clientPort = ResolveClientPort(c.type, c.name);

#ifdef _WINDOWS
    char exePath[MAX_PATH] = {0};
    DWORD n = GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
    {
        XERR("[NodeManager] GetModuleFileNameA failed err=%lu", GetLastError());
        return false;
    }
    std::string full = exePath;
    size_t slash = full.find_last_of("\\/");
    std::string exe = (slash == std::string::npos) ? "RealServer.exe"
                                                   : full.substr(0, slash + 1) + "RealServer.exe";

    std::string cmdline = "\"" + exe + "\"" + " --type=" + std::to_string((int32)c.type)
                          + " --name=" + c.name + " --machine=" + std::to_string(m_machineId)
                          + " --platform=" + m_platform;
    if (clientPort != 0)
    {
        cmdline += " --port=" + std::to_string((int32)clientPort);
    }

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    const char *cwd = m_runRoot.empty() ? nullptr : m_runRoot.c_str();
    BOOL ok = CreateProcessA(nullptr, (LPSTR)cmdline.c_str(), nullptr, nullptr, FALSE,
                             CREATE_NEW_CONSOLE, nullptr, cwd, &si, &pi);
    if (!ok)
    {
        XERR("[NodeManager] CreateProcess failed err=%lu (%s)", GetLastError(), cmdline.c_str());
        return false;
    }
    CloseHandle(pi.hThread);
    c.osPid = pi.dwProcessId;
    c.handle = pi.hProcess;
    c.lastStart = (int64)time(nullptr);
    XLOG("[NodeManager] launched %s type=%d machine=%u pid=%lu", c.name.c_str(), (int32)c.type,
         m_machineId, (unsigned long)pi.dwProcessId);
    return true;
#else
    pid_t pid = fork();
    if (pid < 0)
    {
        XERR("[NodeManager] fork failed errno=%d", errno);
        return false;
    }
    if (pid == 0)
    {
        std::string aType = "--type=" + std::to_string((int32)c.type);
        std::string aName = "--name=" + c.name;
        std::string aMachine = "--machine=" + std::to_string(m_machineId);
        std::string aPlatform = "--platform=" + m_platform;
        if (clientPort != 0)
        {
            std::string aPort = "--port=" + std::to_string((int32)clientPort);
            execl("./RealServer", "RealServer", aType.c_str(), aName.c_str(), aMachine.c_str(),
                  aPlatform.c_str(), aPort.c_str(), (char *)nullptr);
        }
        else
        {
            execl("./RealServer", "RealServer", aType.c_str(), aName.c_str(), aMachine.c_str(),
                  aPlatform.c_str(), (char *)nullptr);
        }
        _exit(127); // exec 失败
    }
    c.osPid = (uint32)pid;
    c.lastStart = (int64)time(nullptr);
    XLOG("[NodeManager] launched %s type=%d machine=%u pid=%u", c.name.c_str(), (int32)c.type,
         m_machineId, c.osPid);
    return true;
#endif
}

bool CNodeManager::LaunchProcess(EProcessType type, const std::string &name)
{
    ChildInfo c;
    c.type = type;
    c.name = name;
    if (!StartChild(c))
    {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_children.push_back(c);
    return true;
}

// =============================================================================
// ReapExitedChildren — 检测退出 (崩溃退避重启 / 优雅停止清理)
// =============================================================================
void CNodeManager::ReapExitedChildren()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto it = m_children.begin(); it != m_children.end();)
    {
        // 待重启且到点 → 就地重启
        if (it->restartAt > 0 && (int64)time(nullptr) >= it->restartAt && !it->graceful)
        {
            it->restartAt = 0;
            if (!StartChild(*it))
            {
                XERR("[NodeManager] relaunch failed %s", it->name.c_str());
                it = m_children.erase(it);
                continue;
            }
            XLOG("[NodeManager] relaunched %s (restart_count=%d)", it->name.c_str(),
                 it->restartCount);
            LogCrash("restarted", it->type, it->name,
                     "restart_count=" + std::to_string(it->restartCount));
            ++it;
            continue;
        }

        // 运行中 → 检测是否退出
        bool exited = false;
#ifdef _WINDOWS
        if (it->handle && WaitForSingleObject(it->handle, 0) == WAIT_OBJECT_0)
        {
            exited = true;
        }
#else
        if (it->osPid)
        {
            int32 status = 0;
            if (waitpid((pid_t)it->osPid, &status, WNOHANG) == (pid_t)it->osPid)
            {
                exited = true;
            }
        }
#endif
        // 优雅停止但超时未退出 → 硬杀兜底 (只杀一次, 清 deadline 避免重复)
        if (!exited && it->graceful && it->gracefulDeadline > 0
            && (int64)time(nullptr) >= it->gracefulDeadline)
        {
            XWRN("[NodeManager] graceful stop timeout — hard kill %s (pid=%u)", it->name.c_str(),
                 it->osPid);
            HardKillChild(*it);
            it->gracefulDeadline = 0;
        }

        if (!exited)
        {
            ++it;
            continue;
        }

#ifdef _WINDOWS
        CloseHandle(it->handle);
        it->handle = nullptr;
#endif

        if (it->graceful)
        {
            XLOG("[NodeManager] child %s (pid=%u) stopped", it->name.c_str(), it->osPid);
            LogCrash("stopped", it->type, it->name,
                     "graceful stop pid=" + std::to_string(it->osPid));
            it = m_children.erase(it);
            continue;
        }

        // 非优雅退出 → 退避重启
        it->restartCount++;
        if (it->restartCount > kMaxRestartCount)
        {
            XERR("[NodeManager] child %s type=%s crashed %d times — faulted, giving up",
                 it->name.c_str(), ProcessTypeName(it->type), it->restartCount);
            PushAlert("child " + it->name + " faulted after " + std::to_string(it->restartCount)
                      + " crashes");
            LogCrash("faulted", it->type, it->name,
                     "crashed " + std::to_string(it->restartCount) + " times, giving up");
            it = m_children.erase(it);
            continue;
        }
        int32 backoff = 1 << it->restartCount;
        if (backoff > kMaxBackoffSec)
        {
            backoff = kMaxBackoffSec;
        }
        it->restartAt = (int64)time(nullptr) + backoff;
        XWRN("[NodeManager] child %s type=%s exited unexpectedly — restart #%d in %ds",
             it->name.c_str(), ProcessTypeName(it->type), it->restartCount, backoff);
        LogCrash("crash", it->type, it->name,
                 "unexpected exit, restart #" + std::to_string(it->restartCount) + " in "
                     + std::to_string(backoff) + "s");
        ++it;
    }
}

// =============================================================================
// 优雅停止子进程 — PUBLISH "shutdown <os_pid>" 到该类型的 GM 频道, 目标进程走
// OnStop (saveAll + deregister) 干净退出; 登记 gracefulDeadline, 由 ReapExitedChildren
// 在超时后硬杀兜底。无 Redis 可用时退化为立即硬杀 (保持旧行为)。
// =============================================================================
void CNodeManager::RequestChildStop(ChildInfo &c)
{
    if (m_cli && c.osPid)
    {
        std::string channel = "cluster:gm:type:" + std::to_string((int32)c.type);
        std::string msg = "shutdown " + std::to_string(c.osPid);
        m_cli->Publish(channel, msg);
        c.gracefulDeadline = (int64)time(nullptr) + kGracefulStopTimeoutSec;
        XLOG("[NodeManager] graceful stop requested: %s (pid=%u, channel=%s)", c.name.c_str(),
             c.osPid, channel.c_str());
    }
    else
    {
        HardKillChild(c);
    }
}

void CNodeManager::HardKillChild(ChildInfo &c)
{
#ifdef _WINDOWS
    if (c.handle)
    {
        TerminateProcess(c.handle, 0);
    }
#else
    if (c.osPid)
    {
        kill((pid_t)c.osPid, SIGTERM);
    }
#endif
}

// =============================================================================
// Redis 读写辅助 (共享 RedisCluster, 线程安全)
// =============================================================================
bool CNodeManager::WriteHash(const std::string &key, const std::string &field,
                             const std::string &val)
{
    if (!m_cli)
    {
        return false;
    }
    return m_cli->HSet(key, field, val) == REDIS_OK;
}

std::string CNodeManager::ReadHash(const std::string &key, const std::string &field)
{
    if (!m_cli)
    {
        return "";
    }
    std::string out;
    if (m_cli->HGet(key, field, out) != REDIS_OK)
    {
        return "";
    }
    return out;
}

void CNodeManager::PushAlert(const std::string &msg)
{
    if (!m_cli)
    {
        return;
    }
    std::string line = std::to_string((int64)time(nullptr))
                       + " [machine:" + std::to_string(m_machineId) + "] " + msg;
    m_cli->RPush(kAlertKey, line);
}

// =============================================================================
// 崩溃日志 (P8) — 写 cluster:crash:log, 格式 {ts}|{machine}|{type}|{name}|{event}|{detail}
// =============================================================================
void CNodeManager::LogCrash(const std::string &event, EProcessType type, const std::string &name,
                            const std::string &detail)
{
    if (!m_cli)
    {
        return;
    }
    std::string line = std::to_string((int64)time(nullptr)) + "|" + std::to_string(m_machineId)
                       + "|" + std::to_string((int32)type) + "|" + name + "|" + event + "|"
                       + detail;
    m_cli->RPush(kCrashLogKey, line);
    m_cli->LTrim(kCrashLogKey, -kCrashLogMax, -1);
}

// =============================================================================
// 自动扩缩容控制器 (P6)
//
// 集群 Leader 选举: 每机器 NodeManager 周期尝试 SETNX cluster:lock:controller
// (TTL 30s), 抢到锁的 NodeManager 成为控制器; 持锁者每次心跳续期, 崩溃后锁自动
// 过期被其他机器接管。控制器评估各类型进程平均 CPU/内存, 越过阈值调整
// cluster:desired:{type}.count (各 NodeManager 的 ReconcileProcesses 随后收敛)。
// =============================================================================
bool CNodeManager::TryAcquireControllerLock()
{
    if (!m_cli)
    {
        return false;
    }
    std::string idStr = std::to_string(m_machineId);

    if (m_cli->SetNXEx(kControllerLockKey, idStr, kControllerLockTtl) == REDIS_OK)
    {
        return true; // 抢锁成功
    }
    // 抢锁失败: 若锁仍是自己持有则续期 (正常续约, 不是新的 Leader 选举)
    std::string holder;
    if (m_cli->Get(kControllerLockKey, holder) == REDIS_OK && holder == idStr)
    {
        m_cli->Expire(kControllerLockKey, kControllerLockTtl);
        return true;
    }
    return false;
}

int32 CNodeManager::ReadInt(const std::string &key, const std::string &field, int32 def)
{
    std::string v = ReadHash(key, field);
    if (v.empty())
    {
        return def;
    }
    return atoi(v.c_str());
}

double CNodeManager::AverageCpu(EProcessType type, int32 &aliveCount)
{
    aliveCount = 0;
    if (!m_cli)
    {
        return 0.0;
    }

    std::string byTypeKey = "cluster:process:by_type:" + std::to_string((int32)type);
    std::vector<std::string> members;
    if (m_cli->SMembers(byTypeKey, members) != REDIS_OK)
    {
        return 0.0;
    }

    int64 now = (int64)time(nullptr);
    int64 cpuSum = 0;
    for (auto &idStr : members)
    {
        std::string procKey = "cluster:process:" + std::to_string((int32)type) + ":" + idStr;
        int64 hb = ReadInt(procKey, "heartbeat", 0);
        if (now - hb > kStaleProcessSec)
        {
            continue; // 僵尸进程不参与评估
        }
        cpuSum += ReadInt(procKey, "cpu_percent", 0);
        ++aliveCount;
    }
    return aliveCount > 0 ? (double)cpuSum / aliveCount : 0.0;
}

void CNodeManager::AutoScale()
{
    if (!m_cli)
    {
        return;
    }

    // 全局开关 (cluster:config:cluster.auto_scale_enabled) — 默认关闭, 手动优先。
    // 只有 WebPlatform 显式置 "1" 才启用自动扩缩容, 避免与手动 desired 互相打架。
    std::string enabled = ReadHash("cluster:config:cluster", "auto_scale_enabled");
    if (enabled != "1")
    {
        return;
    }

    for (EProcessType type : kAllProcessTypes)
    {
        std::string cfgKey = "cluster:config:process:" + std::to_string((int32)type);
        int32 minCount = ReadInt(cfgKey, "min_count", 0);
        int32 maxCount = ReadInt(cfgKey, "max_count", 0);
        int32 cpuFull = ReadInt(cfgKey, "cpu_full", kCpuFullPercentDefault);
        int32 cpuWarn = ReadInt(cfgKey, "cpu_warn", 70);

        int32 desired = ReadInt(DesiredKey(type), "count", 0);
        int32 alive = 0;
        double avgCpu = AverageCpu(type, alive);

        // 上限未知/未配视为不限: 至少要有 min/max 才有自动调节空间
        if (maxCount <= 0)
        {
            maxCount = desired > 0 ? desired + 8 : 8;
        }

        if (avgCpu >= cpuFull && desired < maxCount)
        {
            // 满载 → 扩容 (+1, 每次一拍只加一, 避免过冲)
            int32 next = desired + 1;
            WriteHash(DesiredKey(type), "count", std::to_string(next));
            PushAlert(std::string("auto-scale up: ") + ProcessTypeName(type)
                      + " avg_cpu=" + std::to_string((int32)avgCpu) + " desired "
                      + std::to_string(desired) + "→" + std::to_string(next));
            XLOG("[NodeManager] auto-scale up type=%s avg_cpu=%d desired=%d→%d",
                 ProcessTypeName(type), (int32)avgCpu, desired, next);
        }
        else if (avgCpu < cpuWarn && desired > minCount && minCount > 0)
        {
            // 低载且高于显式配置的下限 → 缩容 (-1)。minCount==0 (未配下限) 时不自动缩,
            // 防止刚拉起 (cpu=0) 的新进程被立即回收导致震荡。
            int32 next = desired - 1;
            WriteHash(DesiredKey(type), "count", std::to_string(next));
            PushAlert(std::string("auto-scale down: ") + ProcessTypeName(type)
                      + " avg_cpu=" + std::to_string((int32)avgCpu) + " desired "
                      + std::to_string(desired) + "→" + std::to_string(next));
            XLOG("[NodeManager] auto-scale down type=%s avg_cpu=%d desired=%d→%d",
                 ProcessTypeName(type), (int32)avgCpu, desired, next);
        }
    }
}

// =============================================================================
// 进程级灰度更新 (P9)
//
// Leader 读 cluster:update:process:config (version/batch_size/status), 当 status=rolling
// 时, 把「上报版本 != 目标版本」的本机子进程按批 graceful 重启 (PUBLISH shutdown, 超时
// 硬杀兜底), 随后 ReconcileProcesses 会按 desired 拉起新进程, 形成滚动替换。换二进制本身
// 由部署侧完成, 此处仅下发「重启以生效」意图。
// =============================================================================
void CNodeManager::ProcessUpdateGray()
{
    if (!m_cli)
    {
        return;
    }

    std::string status = ReadHash("cluster:update:process:config", "status");
    if (status != "rolling")
    {
        return;
    }
    std::string target = ReadHash("cluster:update:process:config", "version");
    if (target.empty())
    {
        return;
    }
    int32 batch = ReadInt("cluster:update:process:config", "batch_size", 1);
    if (batch <= 0)
    {
        batch = 1;
    }

    // 快照本机在运行子进程 (类型+名称), 锁外做网络版本查询
    std::vector<std::pair<EProcessType, std::string>> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto &c : m_children)
        {
            if (!c.graceful)
            {
                snapshot.emplace_back(c.type, c.name);
            }
        }
    }

    std::vector<std::string> stale;
    for (auto &p : snapshot)
    {
        std::string cur = FindChildVersion(p.first, p.second);
        if (!cur.empty() && cur != target)
        {
            stale.push_back(p.second);
        }
    }
    if (stale.empty())
    {
        return;
    }

    int32 done = 0;
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto &c : m_children)
    {
        if (done >= batch)
        {
            break;
        }
        if (c.graceful)
        {
            continue;
        }
        bool isStale = false;
        for (auto &nm : stale)
        {
            if (nm == c.name)
            {
                isStale = true;
                break;
            }
        }
        if (!isStale)
        {
            continue;
        }
        c.graceful = true;
        ++done;
        RequestChildStop(c);
        LogUpdateLog(c.name + " rolling restart → " + target);
        XLOG("[NodeManager] process-update gray: restart %s (target %s)", c.name.c_str(),
             target.c_str());
    }
}

std::string CNodeManager::FindChildVersion(EProcessType type, const std::string &name)
{
    if (!m_cli)
    {
        return "";
    }
    std::string byTypeKey = "cluster:process:by_type:" + std::to_string((int32)type);
    std::vector<std::string> members;
    if (m_cli->SMembers(byTypeKey, members) != REDIS_OK)
    {
        return "";
    }
    int64 now = (int64)time(nullptr);
    for (auto &idStr : members)
    {
        std::string procKey = "cluster:process:" + std::to_string((int32)type) + ":" + idStr;
        if (ReadHash(procKey, "name") != name)
        {
            continue;
        }
        // 只认「心跳新鲜 + 版本非空」的活进程, 跳过 crash 未注销的僵尸条目
        // (否则同名旧条目会返回空版本, 导致灰度判断 cur.empty() 而静默跳过)
        if (now - ReadInt(procKey, "heartbeat", 0) > kStaleProcessSec)
        {
            continue;
        }
        std::string ver = ReadHash(procKey, "version");
        if (ver.empty())
        {
            continue;
        }
        return ver;
    }
    return "";
}

void CNodeManager::LogUpdateLog(const std::string &msg)
{
    if (!m_cli)
    {
        return;
    }
    std::string line = std::to_string((int64)time(nullptr)) + "|process|" + msg;
    m_cli->RPush("cluster:update:process:log", line);
    m_cli->LTrim("cluster:update:process:log", -2000, -1);
}
