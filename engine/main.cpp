/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: main.cpp
      Purpose:   RealServer 进程入口，11 阶段启动流程

      main 先初始化 xLog、加载 config/server.xml，再解析 --type/--name/--port/--platform/--machine/--version 覆盖 process 配置
      构造 CProcessEnv(type) 并 env.Run() 进入 11 阶段启动(身份创建→内核启用→服务注册→Actor/Lua 初始化→运行态)
      Run 主循环为死循环，正常不返回，退出码依 Run 结果取 0 或 1
  *****************************************************************/

#include "xLog/xLog.h"
#include "xConfig/xServerConfig.h"
#include "xEnv/ProcessEnv.h"
#include "xCluster/xProcessDefine.h"

#include <string>
#include <cstdlib>

// =============================================================================
// RealServer 进程入口
//
// 进程启动后是一个"类型化 Actor 运行容器": type 作为启动参数传入
// 命令行参数可覆盖 server.xml 的 <process> 段:
//   RealServer --type=100 --name=gate-01 --platform=dev
//     --type=<100|101|201>  进程类型 (Gate/Login/Game)
//     --name=<name>         进程名称 (e.g. "gate-01")
//     --platform=<dev>      Lua 脚本平台 (dev/test/prod)
// 无参时回退读 config/server.xml。
// 之后由 CProcessEnv 按类型启用内核模块并完成整个启动流程:
//   身份创建 → 内核启用 → 服务注册 → Actor/Lua 初始化 → 运行态
// =============================================================================
int main(int argc, char **argv)
{
    // Phase 1: 日志系统 — 必须先于一切 XLOG 初始化
    xLog::get_mutable_instance().Init();

    // Phase 2: 配置加载 (读取进程类型作为启动参数)
    if (!CServerConfig::getMe().Load("config/server.xml"))
    {
        XERR("[INIT] Failed to load config/server.xml — using defaults");
    }

    // 命令行参数覆盖配置 (argv 优先于 server.xml)
    auto &cfg = CServerConfig::getMe().MutableConfig();
    for (int32 i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg.rfind("--type=", 0) == 0)
        {
            cfg.process.type = (EProcessType)(int32)strtol(arg.c_str() + 7, nullptr, 10);
        }
        else if (arg.rfind("--name=", 0) == 0)
        {
            cfg.process.name = arg.substr(7);
        }
        else if (arg.rfind("--port=", 0) == 0)
        {
            // 业务 TCP 监听端口 (xNetAgent), 供 Gate/Battle 等有 Kernel_Net 的进程
            // 在同一机器上绑定不同客户端接入端口 (多进程共读同一 server.xml)。
            cfg.process.listenPort = (uint16)strtoul(arg.c_str() + 7, nullptr, 10);
        }
        else if (arg.rfind("--platform=", 0) == 0)
        {
            cfg.platform = arg.substr(11);
        }
        else if (arg.rfind("--machine=", 0) == 0)
        {
            cfg.process.machineId = (uint32)strtoul(arg.c_str() + 10, nullptr, 10);
        }
        else if (arg.rfind("--version=", 0) == 0)
        {
            cfg.process.version = arg.substr(10);
        }
    }

    EProcessType type = (EProcessType)cfg.process.type;

    // 构造类型化运行环境并启动 (主循环为死循环, Run 正常不返回)
    CProcessEnv env(type);
    return env.Run() ? 0 : 1;
}
