/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: xProcessManager.h
      Purpose:   进程生命周期管理接口：进程创建、环境初始化、服务注册与心跳。

      声明 CProcessManager 单例：CreateProcess/CreateProcessFromFallback 创建进程身份，InitEnvironment/RegisterInDiscovery 初始化并注册
      StartHeartbeat/StopHeartbeat 管理心跳线程，SetState/RequestShutdown/IsShutdownRequested 状态与优雅关闭
      GetSelfInfo/GetState/GetProcessId 访问器返回 ProcessInfo 自描述
  *****************************************************************/

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
//   1. CreateProcess — Redis 自增 ID + local.info 端口分配 + IP/验证Key + 节点详情确认
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
    bool CreateProcess(EProcessType type, const std::string &name, int64 fallbackId = 1);

    // 使用回退配置创建 (从 server.xml <cluster> 节)
    bool CreateProcessFromFallback(const struct ClusterLocalConfig &localCfg);

    // ---- 环境初始化 ----
    // 标记进程进入 Init 状态 (asio-TCP RPC 服务器/客户端由 ProcessEnv 启动, gRPC 已退役)
    // port: RPC 监听端口
    bool InitEnvironment(uint16 rpcPort);

    // ---- 服务注册 ----
    bool RegisterInDiscovery();

    // ---- 心跳 ----
    void StartHeartbeat();
    void StopHeartbeat();

    // ---- 状态管理 ----
    bool SetState(EProcessState state);

    // ---- 优雅关闭 ----
    // 置关闭标志: OnRunning 主循环检测到后退出, 走 OnStop 优雅关闭序列。
    void RequestShutdown();
    bool IsShutdownRequested() const
    {
        return m_shutdownRequested.load();
    }

    // 安装 Ctrl+C / SIGTERM 处理器 → RequestShutdown (进程内优雅关闭, 替代硬杀)。
    // 注意: Stop-Process -Force / TerminateProcess 是硬杀、不走此回调, 灰度重启须走 GM shutdown。
    void InstallSignalHandlers();

    // ---- 访问器 ----
    const ProcessInfo &GetSelfInfo() const
    {
        return m_stSelfInfo;
    }
    EProcessState GetState() const
    {
        return m_stSelfInfo.state;
    }
    int64 GetProcessId() const
    {
        return m_stSelfInfo.processId;
    }

    bool IsRunning() const
    {
        return m_stSelfInfo.state == EProcessState_Running;
    }

private:
    void HeartbeatLoop(); // 心跳线程函数

private:
    ProcessInfo m_stSelfInfo;
    std::thread m_heartbeatThread;
    std::atomic<bool> m_bHeartbeatRunning{false};
    std::atomic<bool> m_bStopHeartbeat{false};
    std::atomic<bool> m_shutdownRequested{false};
};
