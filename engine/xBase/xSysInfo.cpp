#include "xSysInfo.h"

#ifdef _WINDOWS
#include <windows.h>
#include <psapi.h>
#else
#include <unistd.h>
#include <sys/time.h>
#include <cstdio>
#include <cstring>
#endif

// =============================================================================
// Windows 实现
// =============================================================================
#ifdef _WINDOWS

int32 xSysInfo::GetProcessCpuPercent()
{
    // 基线采样: (进程内核+用户 CPU 时间增量) / 墙钟时间增量 / 核数。
    // GetProcessTimes 与 GetSystemTimeAsFileTime 均以 100ns (FILETIME) 计, 可直接相减。
    static uint64 s_lastProc100ns = 0;
    static uint64 s_lastWall100ns = 0;
    static bool s_hasBaseline = false;

    FILETIME createTime, exitTime, kernelTime, userTime, wallTime;
    GetProcessTimes(GetCurrentProcess(), &createTime, &exitTime, &kernelTime, &userTime);
    GetSystemTimeAsFileTime(&wallTime);

    uint64 proc100ns = ((uint64)kernelTime.dwHighDateTime << 32) | kernelTime.dwLowDateTime;
    proc100ns += ((uint64)userTime.dwHighDateTime << 32) | userTime.dwLowDateTime;
    uint64 wall100ns = ((uint64)wallTime.dwHighDateTime << 32) | wallTime.dwLowDateTime;

    if (!s_hasBaseline)
    {
        s_lastProc100ns = proc100ns;
        s_lastWall100ns = wall100ns;
        s_hasBaseline = true;
        return 0;
    }

    uint64 dProc = proc100ns - s_lastProc100ns;
    uint64 dWall = wall100ns - s_lastWall100ns;
    s_lastProc100ns = proc100ns;
    s_lastWall100ns = wall100ns;

    if (dWall == 0)
    {
        return 0;
    }
    int32 cores = GetMachineCpuCores();
    if (cores <= 0)
    {
        cores = 1;
    }
    double pct = (double)dProc / (double)dWall / (double)cores * 100.0;
    if (pct < 0.0)
    {
        pct = 0.0;
    }
    if (pct > 100.0)
    {
        pct = 100.0;
    }
    return (int32)pct;
}

int64 xSysInfo::GetProcessMemRssMb()
{
    PROCESS_MEMORY_COUNTERS pmc;
    memset(&pmc, 0, sizeof(pmc));
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
    {
        return (int64)(pmc.WorkingSetSize / (1024 * 1024));
    }
    return 0;
}

int32 xSysInfo::GetMachineCpuCores()
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int32)si.dwNumberOfProcessors;
}

int64 xSysInfo::GetMachineTotalMemMb()
{
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms))
    {
        return (int64)(ms.ullTotalPhys / (1024 * 1024));
    }
    return 0;
}

#else
// =============================================================================
// Linux 实现
// =============================================================================

int32 xSysInfo::GetMachineCpuCores()
{
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int32)n : 1;
}

int64 xSysInfo::GetMachineTotalMemMb()
{
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp)
    {
        return 0;
    }
    int64 kb = 0;
    fscanf(fp, "MemTotal: %lld kB", &kb);
    fclose(fp);
    return kb / 1024;
}

int64 xSysInfo::GetProcessMemRssMb()
{
    FILE *fp = fopen("/proc/self/status", "r");
    if (!fp)
    {
        return 0;
    }
    char line[256] = {0};
    int64 kb = 0;
    while (fgets(line, sizeof(line), fp))
    {
        if (sscanf(line, "VmRSS: %lld kB", &kb) == 1)
        {
            break;
        }
    }
    fclose(fp);
    return kb / 1024;
}

int32 xSysInfo::GetProcessCpuPercent()
{
    // /proc/self/stat: utime(第14字段) + stime(第15字段), 单位 jiffies。
    // comm 字段在首对括号内可能含空格, 定位到最后一个 ')' 后再解析后续数字字段。
    static int64 s_lastTicks = 0;
    static int64 s_lastWallUs = 0;
    static bool s_hasBaseline = false;

    FILE *fp = fopen("/proc/self/stat", "r");
    if (!fp)
    {
        return 0;
    }
    char line[1024] = {0};
    if (!fgets(line, sizeof(line), fp))
    {
        fclose(fp);
        return 0;
    }
    fclose(fp);

    char *close = strrchr(line, ')');
    if (!close)
    {
        return 0;
    }

    // close+1 之后依次: state ppid pgrp session tty_nr tpgid flags
    //   minflt cminflt majflt cmajflt utime stime
    uint64 utime = 0, stime = 0;
    int32 n = sscanf(close + 1, "%*c %*ld %*ld %*ld %*ld %*ld %*lu %*lu %*lu %*lu %*lu %llu %llu",
                     &utime, &stime);
    if (n < 2)
    {
        return 0;
    }

    int64 ticks = (int64)utime + (int64)stime;

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int64 wallUs = (int64)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;

    if (!s_hasBaseline)
    {
        s_lastTicks = ticks;
        s_lastWallUs = wallUs;
        s_hasBaseline = true;
        return 0;
    }

    int64 dTicks = ticks - s_lastTicks;
    int64 dWallUs = wallUs - s_lastWallUs;
    s_lastTicks = ticks;
    s_lastWallUs = wallUs;

    if (dWallUs <= 0)
    {
        return 0;
    }
    long clk = sysconf(_SC_CLK_TCK);
    if (clk <= 0)
    {
        clk = 100;
    }
    int32 cores = GetMachineCpuCores();
    // cpu% = (cpu 时间增量秒) / 墙钟增量秒 / 核数 * 100
    double pct = ((double)dTicks / (double)clk) / ((double)dWallUs / 1e6) / (double)cores * 100.0;
    if (pct < 0.0)
    {
        pct = 0.0;
    }
    if (pct > 100.0)
    {
        pct = 100.0;
    }
    return (int32)pct;
}

#endif
