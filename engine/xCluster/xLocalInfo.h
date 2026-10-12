#pragma once

#include "xProcessDefine.h"
#include "xBase/xSingleton.h"
#include <vector>
#include <string>

// =============================================================================
// CLocalInfo — 本机节点注册表 (local.info)
//
// 记录本机运行的进程节点信息, 用于:
//   1. 类型进程初始化时分配一个可用端口 (基础端口 + 递增)
//   2. 记录/查询本机各进程节点的 IP / Port / AuthKey
//
// 文件位于运行根目录 (默认工作目录) 下的 local.info, 一行一节点、空格分隔:
//   type processId name ip rpcPort listenPort authKey state
// =============================================================================
class CLocalInfo : public xSingleton<CLocalInfo>
{
public:
    // 初始化 — rootDir 为运行根目录 (默认 "."), 加载已存在的 local.info
    bool Init(const std::string &rootDir = ".");

    // 为指定类型分配一个可用 RPC 端口 (基础端口 + 首个空闲偏移)
    uint16 AllocatePort(EProcessType type) const;

    // 注册/更新一个节点条目并落盘
    bool Register(const ProcessInfo &info);

    // 移除一个节点条目 (按 processId) 并落盘
    bool Remove(int64 processId);

    // 获取所有已登记节点
    const std::vector<ProcessInfo> &GetNodes() const
    {
        return m_nodes;
    }

    // 获取本机实际 IPv4 地址 (gethostname + getaddrinfo)
    static std::string GetLocalIp();

private:
    void Load();
    bool Save() const;

private:
    std::string m_filePath = "local.info";
    std::vector<ProcessInfo> m_nodes;
};
