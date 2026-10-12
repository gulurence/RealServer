#pragma once

#include "xEnv/BaseEnv.h"

// =============================================================================
// CProcessEnv — 默认进程运行环境
//
// 用现有管理器 (CProcessManager/CActorMgr/CActorScheduler/LuaVM/CNetGateway
// /tServiceMgr 等) 实现 CBaseEnv 的六个阶段。内核模块按 m_uKernel 掩码启用:
//   - Kernel_Net 仅在 Gate 类型启用 → 启动网络接入层
//   - Kernel_MySql 仅在 Login/Player/World 启用 → 配置 MySQL 服务
// =============================================================================
class CProcessEnv : public CBaseEnv
{
public:
    explicit CProcessEnv(EProcessType type) : CBaseEnv(type)
    {
    }

    bool OnInit() override;
    bool OnKernelEnable() override;
    bool OnRegister() override;
    bool OnActorInit() override;
    bool OnRunning() override;
    void OnStop() override;
};
