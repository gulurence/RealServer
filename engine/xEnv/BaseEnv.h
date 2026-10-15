#pragma once

#include "xBase/xDefine.h"
#include "xCluster/xProcessDefine.h"
#include "xLog/xLog.h"

// =============================================================================
// 内核模块 (kernel components) — 进程启动时按类型启用
//
// 进程启动后是一个"类型化 Actor 运行容器": type 作为启动参数传入,
// 不同进程类型 (Gate/Login/Player/World) 启用不同的内核模块集合。
// 用位掩码表达, 由 GetKernelModulesForType(type) 决定。
// =============================================================================
enum EKernelModule : uint32
{
    Kernel_None = 0,
    Kernel_Scheduler = 1u << 0, // 协程 + 同步双线程池调度器
    Kernel_Grpc = 1u << 1,      // gRPC 服务器 + 客户端连接池 (跨进程 RPC)
    Kernel_Redis = 1u << 2,     // Redis 服务 (缓存 + 服务发现)
    Kernel_MySql = 1u << 3,     // MySQL 持久化 (record 模块落库)
    Kernel_Net = 1u << 4,       // 网络接入层 (TCP, Gate 专用)
    Kernel_Nats = 1u << 5,      // NATS 消息 (预留)
    Kernel_Log = 1u << 6,       // 日志服务
    Kernel_Xml = 1u << 7,       // XML 配置服务
    Kernel_Platform = 1u << 8,  // 平台服务
};

// 按进程类型返回启用的内核模块集合
inline uint32 GetKernelModulesForType(EProcessType type)
{
    switch (type)
    {
    case EProcessType_PGate:
        // 网关: 网络接入 + 无持久化 (不需要 MySQL)
        return Kernel_Scheduler | Kernel_Grpc | Kernel_Redis | Kernel_Net | Kernel_Log | Kernel_Xml
               | Kernel_Platform;
    case EProcessType_PLogin:
    case EProcessType_PGlobal:
    case EProcessType_PGame:
        // 有状态进程: 需要 MySQL 持久化, 无网络接入
        return Kernel_Scheduler | Kernel_Grpc | Kernel_Redis | Kernel_MySql | Kernel_Log
               | Kernel_Xml | Kernel_Platform;
    case EProcessType_PBattle:
        // 战斗进程: 客户端直连 (Kernel_Net), 战斗花名册/鉴权走 Redis, 无 MySQL 持久化
        return Kernel_Scheduler | Kernel_Grpc | Kernel_Redis | Kernel_Net | Kernel_Log
               | Kernel_Xml | Kernel_Platform;
    default:
        return Kernel_Scheduler | Kernel_Grpc | Kernel_Log | Kernel_Xml;
    }
}

inline bool HasKernelModule(uint32 u32Set, EKernelModule eModule)
{
    return (u32Set & eModule) != 0;
}

// =============================================================================
// CBaseEnv — 进程运行环境抽象
//
// 生命周期主流程 Run() 按序调用各阶段 (模板方法模式):
//   OnInit          — 配置加载 + 进程身份创建
//   OnKernelEnable  — 按 m_uKernel 启用内核模块 (调度器/gRPC/三方服务)
//   OnRegister      — 服务发现注册 + 心跳
//   OnActorInit     — Actor 管理器 + Lua 层初始化
//   OnRunning       — 进入运行态 (网络接入 + 标记 Running + 主循环)
//   OnStop          — 优雅关闭 (主循环为死循环, 正常不可达)
//
// 不同类型可派生子类覆写任一阶段; 默认实现见 CProcessEnv。
// =============================================================================
class CBaseEnv
{
public:
    explicit CBaseEnv(EProcessType type) : m_eType(type), m_uKernel(GetKernelModulesForType(type))
    {
    }
    virtual ~CBaseEnv() = default;

    // 生命周期主流程 (任一阶段失败返回 false)
    bool Run()
    {
        if (!OnInit())
        {
            XERR("[BaseEnv:%s] OnInit failed", ProcessTypeName(m_eType));
            return false;
        }
        if (!OnKernelEnable())
        {
            XERR("[BaseEnv:%s] OnKernelEnable failed", ProcessTypeName(m_eType));
            return false;
        }
        if (!OnRegister())
        {
            // 服务发现注册失败不致命 — 以 standalone 继续
            XWRN("[BaseEnv:%s] OnRegister failed — continuing standalone",
                 ProcessTypeName(m_eType));
        }
        if (!OnActorInit())
        {
            XERR("[BaseEnv:%s] OnActorInit failed", ProcessTypeName(m_eType));
            return false;
        }
        if (!OnRunning())
        {
            XERR("[BaseEnv:%s] OnRunning failed", ProcessTypeName(m_eType));
            return false;
        }
        OnStop();
        return true;
    }

    // ---- 各阶段 (子类覆写) ----
    virtual bool OnInit() = 0;
    virtual bool OnKernelEnable() = 0;
    virtual bool OnRegister() = 0;
    virtual bool OnActorInit() = 0;
    virtual bool OnRunning() = 0;
    virtual void OnStop() = 0;

    // ---- 访问器 ----
    EProcessType GetType() const
    {
        return m_eType;
    }
    uint32 GetKernelModules() const
    {
        return m_uKernel;
    }
    bool HasModule(EKernelModule m) const
    {
        return HasKernelModule(m_uKernel, m);
    }

protected:
    EProcessType m_eType;
    uint32 m_uKernel;
};
