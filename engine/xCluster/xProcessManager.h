#pragma once

#include "xProcessDefine.h"
#include "xBase/xSingleton.h"
#include <string>
#include <thread>
#include <atomic>

// =============================================================================
// CProcessManager — 进程生命周期管理
//
// 流程:
//   1. CreateProcess — Redis 获取配置 + 生成 ID, 失败则用本地回退
//   2. InitEnvironment — 初始化调度器/gRPC/第三方服务
//   3. RegisterInDiscovery — Redis 服务注册
//   4. StartHeartbeat — 启动 5 秒心跳定时器
// =============================================================================
class CProcessManager : public xSingleton<CProcessManager>
{
public:
    // ---- 进程创建 ----
    // type: 进程类型 (EProcessType)
    // name: 进程名称 (e.g. "gate-01")
    // fallbackId: Redis 不可用时的回退 ID
    // 返回: true=成功 (使用 Redis 或回退)
    bool CreateProcess(EProcessType type, const std::string& name, int64 fallbackId = 1);

    // 使用回退配置创建 (从 server.xml <cluster> 节)
    bool CreateProcessFromFallback(const struct ClusterLocalConfig& localCfg);

    // ---- 环境初始化 ----
    // 初始化调度器 + gRPC 服务器 + 注册 ActorMgr 服务
    // port: gRPC 监听端口
    bool InitEnvironment(uint16 grpcPort);

    // ---- 服务注册 ----
    bool RegisterInDiscovery();

    // ---- 心跳 ----
    void StartHeartbeat();
    void StopHeartbeat();

    // ---- 状态管理 ----
    bool SetState(EProcessState state);

    // ---- 访问器 ----
    const ProcessInfo& GetSelfInfo() const { return m_stSelfInfo; }
    EProcessState GetState() const { return m_stSelfInfo.state; }
    int64 GetProcessId() const { return m_stSelfInfo.processId; }

    bool IsRunning() const { return m_stSelfInfo.state == EProcessState_Running; }

private:
    void HeartbeatLoop();    // 心跳线程函数

private:
    ProcessInfo     m_stSelfInfo;
    std::thread     m_heartbeatThread;
    std::atomic<bool> m_bHeartbeatRunning{false};
    std::atomic<bool> m_bStopHeartbeat{false};
};
