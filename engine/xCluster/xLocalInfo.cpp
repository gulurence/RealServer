#include "xLocalInfo.h"
#include "xLog/xLog.h"

#include <fstream>
#include <sstream>
#include <set>
#include <algorithm>

// =============================================================================
// Init — 设置运行根目录并加载已存在的 local.info
// =============================================================================
bool CLocalInfo::Init(const std::string &rootDir)
{
    if (rootDir.empty() || rootDir == ".")
    {
        m_filePath = "local.info";
    }
    else
    {
        m_filePath = rootDir + "/local.info";
    }
    Load();
    return true;
}

// =============================================================================
// Load — 解析 local.info (一行一节点, 空格分隔, # 为注释)
// =============================================================================
void CLocalInfo::Load()
{
    m_nodes.clear();
    std::ifstream fin(m_filePath);
    if (!fin.is_open())
    {
        XLOG("[LocalInfo] no existing %s — starting empty", m_filePath.c_str());
        return;
    }

    std::string line;
    while (std::getline(fin, line))
    {
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        std::istringstream iss(line);
        ProcessInfo info;
        int32 type = 0;
        int64 pid = 0;
        uint32 rpcPort = 0;
        uint32 listenPort = 0;
        int32 state = 0;
        std::string name, ip, authKey;

        iss >> type >> pid >> name >> ip >> rpcPort >> listenPort >> authKey >> state;
        if (pid <= 0)
        {
            continue; // 跳过无效行
        }

        info.type = (EProcessType)type;
        info.processId = pid;
        info.processName = name;
        info.ip = ip;
        info.rpcPort = (uint16)rpcPort;
        info.listenPort = (uint16)listenPort;
        info.authKey = authKey;
        info.state = (EProcessState)state;
        m_nodes.push_back(info);
    }
    XLOG("[LocalInfo] loaded %zu node(s) from %s", m_nodes.size(), m_filePath.c_str());
}

// =============================================================================
// Save — 全量落盘
// =============================================================================
bool CLocalInfo::Save() const
{
    std::ofstream fout(m_filePath, std::ios::trunc);
    if (!fout.is_open())
    {
        XERR("[LocalInfo] failed to open %s for write", m_filePath.c_str());
        return false;
    }
    fout << "# RealServer 本机节点注册表（引擎自动维护）\n";
    fout << "# type processId name ip rpcPort listenPort authKey state\n";
    for (auto &n : m_nodes)
    {
        fout << (int32)n.type << ' ' << n.processId << ' ' << n.processName << ' ' << n.ip << ' '
             << n.rpcPort << ' ' << n.listenPort << ' ' << n.authKey << ' ' << (int32)n.state
             << '\n';
    }
    fout.flush();
    bool ok = fout.good();
    if (!ok)
    {
        XERR("[LocalInfo] failed to write %s", m_filePath.c_str());
    }
    return ok;
}

// =============================================================================
// AllocatePort — 类型基础端口 + 首个空闲偏移
// =============================================================================
uint16 CLocalInfo::AllocatePort(EProcessType type) const
{
    uint16 base = GetBasePortForType(type);
    if (base == 0)
    {
        XERR("[LocalInfo] AllocatePort: unknown process type=%d, no base port", (int32)type);
        return 0;
    }

    std::set<uint16> used;
    for (auto &n : m_nodes)
    {
        if (n.type == type)
        {
            used.insert(n.rpcPort);
        }
    }

    for (uint16 offset = 0; offset < 50; ++offset)
    { // 每类型预留 50 端口
        uint16 candidate = base + offset;
        if (used.count(candidate) == 0)
        {
            return candidate;
        }
    }

    XWRN("[LocalInfo] port range exhausted for type=%d (base=%u)", (int32)type, base);
    return base;
}

// =============================================================================
// Register / Remove
// =============================================================================
bool CLocalInfo::Register(const ProcessInfo &info)
{
    for (auto &n : m_nodes)
    {
        if (n.type == info.type && n.processId == info.processId)
        {
            n = info; // 已存在 → 更新
            return Save();
        }
    }
    m_nodes.push_back(info); // 新增
    return Save();
}

bool CLocalInfo::Remove(int64 processId)
{
    size_t before = m_nodes.size();
    m_nodes.erase(std::remove_if(m_nodes.begin(), m_nodes.end(),
                                 [&](const ProcessInfo &n) { return n.processId == processId; }),
                  m_nodes.end());
    if (m_nodes.size() == before)
    {
        return true; // 未找到, 视为成功
    }
    return Save();
}

// =============================================================================
// GetLocalIp — 本机实际 IPv4 (gethostname + gethostbyname)
//
// 返回主网卡的 IPv4; 若主机名解析到虚拟网卡(如 VPN)可能不准, 属已知局限。
// =============================================================================
std::string CLocalInfo::GetLocalIp()
{
#ifdef _WINDOWS
    WSADATA wsa;
    static bool s_wsaInited = false;
    if (!s_wsaInited)
    {
        if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0)
        {
            s_wsaInited = true;
        }
    }
#endif

    char hostname[256] = {0};
    if (gethostname(hostname, sizeof(hostname)) != 0)
    {
        XWRN("[LocalInfo] gethostname failed — fallback 127.0.0.1");
        return "127.0.0.1";
    }

    struct hostent *he = gethostbyname(hostname);
    if (!he || !he->h_addr_list || !he->h_addr_list[0])
    {
        XWRN("[LocalInfo] gethostbyname failed for '%s' — fallback 127.0.0.1", hostname);
        return "127.0.0.1";
    }

    struct in_addr addr;
    memcpy(&addr, he->h_addr_list[0], sizeof(addr));
    const char *ip = inet_ntoa(addr);
    return ip ? ip : "127.0.0.1";
}
