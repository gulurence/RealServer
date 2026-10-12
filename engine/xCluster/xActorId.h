#pragma once

#include "xBase/xDefine.h"

#ifndef _WINDOWS
#include <unistd.h> // getpid (POSIX)
#endif

// =============================================================================
// xActorId — 64 位全局 ActorID / ProcessID 编解码
//
// 位布局 (高 → 低), 全部用 uint64 存储:
//
//   ActorID[64位]   = MachineID[24位] + PID[16位] + ActorIndex[24位]
//                      ┌──── bit63 ──────────────────────── bit0 ────┐
//                      │ [63..40] MachineID (24)                     │
//                      │ [39..24] PID       (16)                     │
//                      │ [23.. 0] ActorIndex(24)                     │
//                      └─────────────────────────────────────────────┘
//
//   ProcessID[64位] = 0[高24位暂保留] + MachineID[24位] + PID[16位]
//                      ┌──── bit63 ──────────────────────── bit0 ────┐
//                      │ [63..40] 0          (24, 暂保留)            │
//                      │ [39..16] MachineID  (24)                    │
//                      │ [15.. 0] PID        (16)                    │
//                      └─────────────────────────────────────────────┘
//
// 注意: ActorID 与 ProcessID 的 MachineID/PID 位偏移不同 (ActorID 中 MachineID
// 在 bit40、ProcessID 中在 bit16), 二者转换必须走 ActorIdToProcessId 显式重组。
//
// 自描述: 从 ActorID 直接解析出 MachineID + PID 即可定位承载进程, 无需再查
// 「Actor → 进程」索引即可路由; ActorIndex 定位进程内 Actor 实例。
// =============================================================================
namespace xActorId
{

// ---- 位宽 / 位移 / 掩码 ----
constexpr uint32 kMachineIdBits = 24;
constexpr uint32 kPidBits = 16;
constexpr uint32 kActorIndexBits = 24;

// ActorID 内各段位移 (自低位起)
constexpr uint32 kActorIndexShift = 0;
constexpr uint32 kPidShift = kActorIndexShift + kActorIndexBits; // 24
constexpr uint32 kMachineIdShift = kPidShift + kPidBits;         // 40

constexpr uint64 kActorIndexMask = ((uint64)1 << kActorIndexBits) - 1; // 0xFFFFFF
constexpr uint64 kPidMask = ((uint64)1 << kPidBits) - 1;               // 0xFFFF
constexpr uint64 kMachineIdMask = ((uint64)1 << kMachineIdBits) - 1;   // 0xFFFFFF

// ProcessID 内各段位移 (与 ActorID 布局不同: 高 24 位保留为 0)
constexpr uint32 kProcPidShift = 0;
constexpr uint32 kProcMachineIdShift = kPidBits; // 16

// =============================================================================
// 生成 (打包)
// =============================================================================

// 由 MachineID + PID + ActorIndex 打包成全局 ActorID
inline uint64 MakeActorId(uint32 machineId, uint32 pid, uint32 actorIndex)
{
    return (((uint64)(machineId & kMachineIdMask)) << kMachineIdShift)
           | (((uint64)(pid & kPidMask)) << kPidShift) | ((uint64)(actorIndex & kActorIndexMask));
}

// 由 MachineID + PID 打包成 ProcessID (高 24 位保留为 0)
inline uint64 MakeProcessId(uint32 machineId, uint32 pid)
{
    return (((uint64)(machineId & kMachineIdMask)) << kProcMachineIdShift)
           | ((uint64)(pid & kPidMask));
}

// =============================================================================
// 解析 (拆解)
// =============================================================================

inline uint32 GetMachineId(uint64 actorId)
{
    return (uint32)((actorId >> kMachineIdShift) & kMachineIdMask);
}

inline uint32 GetPid(uint64 actorId)
{
    return (uint32)((actorId >> kPidShift) & kPidMask);
}

inline uint32 GetActorIndex(uint64 actorId)
{
    return (uint32)((actorId >> kActorIndexShift) & kActorIndexMask);
}

// 从 ActorID 提取其所属 ProcessID (按 ProcessID 布局重组: MachineID<<16 | PID)
inline uint64 ActorIdToProcessId(uint64 actorId)
{
    return MakeProcessId(GetMachineId(actorId), GetPid(actorId));
}

// 判断 ActorID 是否携带合法 ActorIndex (实例索引从 1 开始, 0 = 非法)
inline bool IsValidActorId(uint64 actorId)
{
    return actorId != 0 && GetActorIndex(actorId) != 0;
}

// 操作系统 PID (Windows GetCurrentProcessId / POSIX getpid)
// 注: PID 字段仅 16 位, OS PID 超 65535 时会被打包时截断 (见潜在问题)。
inline uint32 GetOsPid()
{
#ifdef _WINDOWS
    return (uint32)GetCurrentProcessId();
#else
    return (uint32)getpid();
#endif
}

} // namespace xActorId
