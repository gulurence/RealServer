/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: xSysInfo.h
      Purpose:   跨平台系统与进程资源采样类

      声明 xSysInfo 静态工具类
      提供进程 CPU/内存与整机核数/内存采样，CPU 百分比带基线、内存单位 MB
  *****************************************************************/

#pragma once

#include "xBase/xDefine.h"

// =============================================================================
// xSysInfo — 跨平台系统/进程资源采样工具
//
// 用途:
//   - 进程心跳上报本进程 CPU/内存占用 (xProcessManager 心跳线程)
//   - NodeManager 上报物理机容量 (cpu_cores / mem_total)
//
// Windows: GetProcessTimes / GetProcessMemoryInfo / GetSystemInfo /
//          GlobalMemoryStatusEx
// Linux:   /proc/self/stat / /proc/self/status / /proc/meminfo / sysconf
//
// 单位约定: CPU 百分比(0-100 整数), 内存 MB。
// CPU 采样带基线: 首次调用无基线返回 0, 之后按两次调用间隔的 CPU 时间/墙钟时间比值计算。
// =============================================================================
class xSysInfo
{
public:
    // 本进程 CPU 占用百分比 (0-100)。首次调用返回 0 (建立基线)。
    static int32 GetProcessCpuPercent();

    // 本进程常驻内存 RSS (MB)。
    static int64 GetProcessMemRssMb();

    // 本机 CPU 逻辑核数。
    static int32 GetMachineCpuCores();

    // 本机物理内存总量 (MB)。
    static int64 GetMachineTotalMemMb();
};
