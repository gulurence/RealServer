/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: main.cpp
      Purpose:   NodeManager 守护进程入口

      实例化 CNodeManager 并依次调用 Init 与 Run
      Run 为死循环正常不返回，退出后调用 Shutdown 收尾
  *****************************************************************/

#include "NodeManager.h"

// =============================================================================
// NodeManager 进程入口 — 物理机守护进程 (独立轻量二进制)
//
// 每台物理机一个, 负责机器注册/心跳、按期望数起停 RealServer 子进程、
// 崩溃拉起、本机容量上报。只链接 xBase + xRedis, 不链接 Lua/Actor/RPC。
//
// 启动: NodeManager --machine-id=<id> --platform=dev --config=config/server.xml [--ip=<ip>]
// 与 RealServer.exe 同目录产出 (负责拉起后者)。
// =============================================================================
int main(int argc, char **argv)
{
    CNodeManager mgr;
    if (!mgr.Init(argc, argv))
    {
        return 1;
    }
    mgr.Run(); // 死循环, 正常不返回
    mgr.Shutdown();
    return 0;
}
