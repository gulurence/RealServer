#pragma once

#include "xProcessDefine.h"

#include <cstdlib>
#include <string>

// =============================================================================
// xProcessMetrics — 进程监控上报字段定义表 (定义与逻辑分离)
//
// 每个字段一行: Redis hash 字段名 + 序列化取值 (get) + 反序列化回填 (set)。
// WriteProcessInfo / ReadProcessInfo 只遍历这张表, 不再逐字段硬编码 hset/hget。
// 新增监控字段只需在此表加一行 (如 WebPlatform 需展示则再改 www/ 页面)。
// =============================================================================
namespace xProcessMetrics
{

struct ProcessMetricDef
{
    const char *field;                               // Redis hash 字段名
    std::string (*get)(const ProcessInfo &);         // 序列化: 字段 → 字符串
    void (*set)(ProcessInfo &, const std::string &); // 反序列化: 字符串 → 字段
};

// ---- 序列化 (get) ----
inline std::string GetType(const ProcessInfo &i) { return std::to_string((int32)i.type); }
inline std::string GetId(const ProcessInfo &i) { return std::to_string(i.processId); }
inline std::string GetName(const ProcessInfo &i) { return i.processName; }
inline std::string GetHost(const ProcessInfo &i) { return i.host; }
inline std::string GetIp(const ProcessInfo &i) { return i.ip; }
inline std::string GetPort(const ProcessInfo &i) { return std::to_string(i.rpcPort); }
inline std::string GetListenPort(const ProcessInfo &i) { return std::to_string(i.listenPort); }
inline std::string GetLoad(const ProcessInfo &i) { return std::to_string(i.loadScore); }
inline std::string GetMachineId(const ProcessInfo &i) { return std::to_string(i.machineId); }
inline std::string GetCpuPercent(const ProcessInfo &i) { return std::to_string(i.cpuPercent); }
inline std::string GetMemRssMb(const ProcessInfo &i) { return std::to_string(i.memRssMb); }
inline std::string GetMemPercent(const ProcessInfo &i) { return std::to_string(i.memPercent); }
inline std::string GetVersion(const ProcessInfo &i) { return i.version; }
inline std::string GetContentVersion(const ProcessInfo &i) { return i.contentVersion; }
inline std::string GetState(const ProcessInfo &i) { return std::to_string((int32)i.state); }
inline std::string GetHeartbeat(const ProcessInfo &i) { return std::to_string(i.lastHeartbeat); }
inline std::string GetAuthKey(const ProcessInfo &i) { return i.authKey; }
inline std::string GetOsPid(const ProcessInfo &i) { return std::to_string(i.osPid); }
inline std::string GetDrain(const ProcessInfo &i) { return i.draining ? "1" : "0"; }

// ---- 反序列化 (set) ----
inline void SetType(ProcessInfo &i, const std::string &v) { i.type = (EProcessType)atoi(v.c_str()); }
inline void SetId(ProcessInfo &i, const std::string &v) { i.processId = atoll(v.c_str()); }
inline void SetName(ProcessInfo &i, const std::string &v) { i.processName = v; }
inline void SetHost(ProcessInfo &i, const std::string &v) { i.host = v; }
inline void SetIp(ProcessInfo &i, const std::string &v) { i.ip = v; }
inline void SetPort(ProcessInfo &i, const std::string &v) { i.rpcPort = (uint16)atoi(v.c_str()); }
inline void SetListenPort(ProcessInfo &i, const std::string &v) { i.listenPort = (uint16)atoi(v.c_str()); }
inline void SetLoad(ProcessInfo &i, const std::string &v) { i.loadScore = atoi(v.c_str()); }
inline void SetMachineId(ProcessInfo &i, const std::string &v) { i.machineId = (uint32)atoi(v.c_str()); }
inline void SetCpuPercent(ProcessInfo &i, const std::string &v) { i.cpuPercent = atoi(v.c_str()); }
inline void SetMemRssMb(ProcessInfo &i, const std::string &v) { i.memRssMb = atoll(v.c_str()); }
inline void SetMemPercent(ProcessInfo &i, const std::string &v) { i.memPercent = atoi(v.c_str()); }
inline void SetVersion(ProcessInfo &i, const std::string &v) { i.version = v; }
inline void SetContentVersion(ProcessInfo &i, const std::string &v) { i.contentVersion = v; }
inline void SetState(ProcessInfo &i, const std::string &v) { i.state = (EProcessState)atoi(v.c_str()); }
inline void SetHeartbeat(ProcessInfo &i, const std::string &v) { i.lastHeartbeat = atoll(v.c_str()); }
inline void SetAuthKey(ProcessInfo &i, const std::string &v) { i.authKey = v; }
inline void SetOsPid(ProcessInfo &i, const std::string &v) { i.osPid = atoll(v.c_str()); }
inline void SetDrain(ProcessInfo &i, const std::string &v) { i.draining = (v == "1"); }

// ---- 字段定义表 (顺序即 Redis hash 写入顺序) ----
inline constexpr ProcessMetricDef kProcessMetrics[] = {
    {"type", GetType, SetType},
    {"id", GetId, SetId},
    {"name", GetName, SetName},
    {"host", GetHost, SetHost},
    {"ip", GetIp, SetIp},
    {"port", GetPort, SetPort},
    {"listen_port", GetListenPort, SetListenPort},
    {"load", GetLoad, SetLoad},
    {"machine_id", GetMachineId, SetMachineId},
    {"cpu_percent", GetCpuPercent, SetCpuPercent},
    {"mem_rss_mb", GetMemRssMb, SetMemRssMb},
    {"mem_percent", GetMemPercent, SetMemPercent},
    {"version", GetVersion, SetVersion},
    {"content_version", GetContentVersion, SetContentVersion},
    {"state", GetState, SetState},
    {"heartbeat", GetHeartbeat, SetHeartbeat},
    {"authkey", GetAuthKey, SetAuthKey},
    {"os_pid", GetOsPid, SetOsPid},
    {"drain", GetDrain, SetDrain},
};

} // namespace xProcessMetrics
