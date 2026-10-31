/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: NodeManager.h
      Purpose:   物理机守护进程 CNodeManager 类声明

      声明 CNodeManager 守护进程类与 ChildInfo 子进程状态结构
      定义机器注册心跳、期望数收敛、自动扩缩容、灰度更新等接口与各 Redis 键常量
  *****************************************************************/

#pragma once

#include "xBase/xDefine.h"
#include "xRedis/RedisCli.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#ifdef _WINDOWS
#include <windows.h>
#endif

// =============================================================================
// CNodeManager — 物理机守护进程 (独立轻量二进制 NodeManager)
//
// 每台物理机一个, 只链接 xBase + xRedis (RedisCluster), 不链接 Lua/Actor/RPC。
// 职责 (对齐设计文档 docs/cluster-orchestration-design.md §4.1):
//   1. 机器注册 + 心跳     → cluster:node:{machineId} Hash + cluster:node:all Set
//   2. 期望数收敛           → 读 cluster:desired:{type} 起停 RealServer 子进程
//   3. 崩溃拉起             → 非优雅退出退避重启 (1s→2s→4s…封顶 60s)
//   4. 本机容量上报         → cpu_cores / mem_total_mb
//   5. 容量上限             → 依 cpu/mem 折算本机进程总量上限, 超限不再起新进程
//
// 启动: NodeManager --machine-id=<id> --platform=dev --config=config/server.xml [--ip=<ip>]
// =============================================================================
class CNodeManager
{
private:
    struct ChildInfo; // 前置声明 (完整定义见下方 private 段)

public:
    bool Init(int32 argc, char **argv);
    void Run(); // 主循环 (死循环, 周期收敛 + 心跳 + 崩溃拉起)
    void Shutdown();

private:
    bool ParseArgs(int32 argc, char **argv);
    void ResolveRunRoot(); // 定位仓库根目录 (config/ + script/), 双击启动时修正 cwd
    bool LoadConfig();     // 读 server.xml 拿 Redis seed + auth
    uint32 ResolveMachineId(); // --machine-id 优先, 否则本机文件缓存
    std::string GetHostname();

    void RegisterMachine();
    void HeartbeatMachine();
    void ReconcileProcesses(); // 期望数收敛 (起/停)
    int32 CountAlive(EProcessType type) const;
    bool CapacityOk(); // 本机容量上限校验 (读 Redis 配置, 非 const)

    bool LaunchProcess(EProcessType type, const std::string &name);
    bool StartChild(ChildInfo &c); // 实际 CreateProcess/fork (填充 handle/osPid/lastStart)
    void ReapExitedChildren();     // 检测退出 → 退避重启 / 清理
    void RequestChildStop(ChildInfo &c); // PUBLISH shutdown <os_pid> + 登记硬杀兜底截止时间
    void HardKillChild(ChildInfo &c);    // TerminateProcess / kill(SIGTERM) 兜底
    std::string NextName(EProcessType type) const; // 生成 {TypeName}-{seq} 名称

    // 自动扩缩容控制器 (P6: Leader 选举 + 按类型资源评估改 desired)
    bool TryAcquireControllerLock(); // SETNX cluster:lock:controller + 续期
    void AutoScale(); // 读 cluster:config:process:{type} + 进程资源, 改 desired
    double AverageCpu(EProcessType type, int32 &aliveCount); // 采集某类型在线进程平均 CPU
    int32 ReadInt(const std::string &key, const std::string &field, int32 def);

    // 进程级灰度更新 (P9: 读 cluster:update:process:config, 分批滚动重启旧版本进程)
    void ProcessUpdateGray();
    std::string FindChildVersion(EProcessType type, const std::string &name);
    void LogUpdateLog(const std::string &msg); // 写 cluster:update:process:log

    // 崩溃日志 (P8: 写 cluster:crash:log)
    void LogCrash(const std::string &event, EProcessType type, const std::string &name,
                  const std::string &detail);

    // Redis 读写 (走共享 RedisCluster)
    bool WriteHash(const std::string &key, const std::string &field, const std::string &val);
    std::string ReadHash(const std::string &key, const std::string &field);
    void PushAlert(const std::string &msg); // 写 cluster:alert

private:
    struct ChildInfo
    {
        EProcessType type = EProcessType_None;
        std::string name;
        uint32 osPid = 0; // 操作系统 PID
#ifdef _WINDOWS
        HANDLE handle = nullptr;
#endif
        int32 restartCount = 0; // 连续崩溃重启次数
        int64 lastStart = 0;    // 最近一次启动 unix 秒
        int64 restartAt = 0;    // 退避重启时间点 (unix 秒, 0=无待重启)
        bool graceful = false;  // true=期望停止, 不再拉起
        int64 gracefulDeadline = 0; // 优雅停止硬杀兜底截止时间 (unix 秒, 0=未请求)
    };

    uint32 m_machineId = 0;
    std::string m_platform = "dev";
    std::string m_configPath = "config/server.xml";
    std::string m_runRoot; // 解析出的仓库根目录 (含 config/ 与 script/)
    std::string m_ip = "127.0.0.1";
    std::string m_hostname;
    std::string m_redisTitle = "cluster_discovery";

    CRedisCli *m_cli = nullptr;

    std::vector<ChildInfo> m_children;
    mutable std::mutex m_mutex; // const 方法 (CountAlive/CapacityOk) 也需加锁

    std::atomic<bool> m_running{false};
    int64 m_lastHeartbeat = 0;
    int64 m_lastReconcile = 0;
    int64 m_lastAutoScale = 0; // 自动扩缩容控制器周期 (秒)
};
