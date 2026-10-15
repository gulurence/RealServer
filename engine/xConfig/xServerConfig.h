#pragma once

#include "xBase/xSingleton.h"
#include "xCluster/xProcessDefine.h"
#include "tService/tThridServiceConfig.h"
#include <vector>
#include <string>

// =============================================================================
// 进程配置 — 标识本进程的角色和身份
// =============================================================================
struct ProcessXmlConfig
{
    EProcessType type = EProcessType_PGate; // 进程类型 (PGate/PLogin/PGlobal/PGame)
    std::string name = "default";           // 进程名称 (e.g. "gate-01")
    std::string host = "0.0.0.0";           // 监听地址
    uint16 listenPort = 10000;              // 业务 TCP 监听端口 (xNetAgent)
    uint32 machineId = 0; // 所属物理机 (NodeManager 分配, --machine 传入; 单机=0)
    std::string version; // 进程二进制版本 (--version 传入; 空则回退 kProcessVersion)
    std::vector<EActorType>
        hostedActors; // <process><hosted-actors> 嵌套声明 (可选, 本进程可承载 Actor 类型)
};

// =============================================================================
// Hosted Actors 声明 — 某进程类型可承载的 Actor 类型列表
// 对应 xProcessDefine.h 硬编码 GetHostedActorTypes, server.xml 可覆盖之
// =============================================================================
struct HostedActorsConfig
{
    EProcessType processType = EProcessType_None; // 进程类型
    std::vector<EActorType> actorTypes;           // 该进程类型可承载的 Actor 类型列表
};

// =============================================================================
// 集群本地回退配置 — 当 Redis 不可用时使用
// =============================================================================
struct ClusterLocalConfig
{
    EProcessType type = EProcessType_PGate;
    int64 processId = 1;
    std::string name = "local";
};

struct ClusterPeerConfig
{
    EProcessType type = EProcessType_PLogin;
    std::string host = "127.0.0.1";
    uint16 port = 50051;
};

struct ClusterFallbackConfig
{
    ClusterLocalConfig local;
    std::vector<ClusterPeerConfig> peers;
};

// =============================================================================
// 服务发现连接池配置
// =============================================================================
struct DiscoveryRedisConfig
{
    std::string poolTitle = "cluster_discovery"; // Redis 池名称
};

// Holds all server configuration parsed from server.xml at startup.
// Designed as a POCO (Plain Old Config Object) consumed by main.cpp.
struct ServerXmlConfig
{
    uint16 rpcPort = 50051;
    std::string platform = "dev";                 // dev / test / prod
    std::vector<ThridServiceConfig> redisConfigs; // Redis pool configs
    std::vector<ThridServiceConfig> dbConfigs;    // DB pool configs

    // 进程标识配置
    ProcessXmlConfig process;
    // hosted-actors 声明表 (顶层 <hosted-actors>, 进程类型 → 可承载 Actor 列表)
    std::vector<HostedActorsConfig> hostedActors;
    // 集群回退配置 (Redis 不可用时)
    ClusterFallbackConfig clusterFallback;
    // 服务发现 Redis 池
    DiscoveryRedisConfig discoveryRedis;
    // 验证 Key 派生密钥 (来自 <cluster><auth secret="..."/>) 缺省用代码常量
    std::string authSecret;
};

// CServerConfig — singleton configuration loader.
class CServerConfig : public xSingleton<CServerConfig>
{
public:
    bool Load(const std::string &strXmlPath);

    // 是否已加载过 server.xml (防止 tServiceXml 等模块重复 Load 覆盖命令行参数)
    bool IsLoaded() const
    {
        return m_bLoaded;
    }

    const ServerXmlConfig &GetConfig() const
    {
        return m_stConfig;
    }
    ServerXmlConfig &MutableConfig()
    {
        return m_stConfig;
    }

    const ThridServiceConfig *FindRedisCfg(const std::string &title) const;
    const ThridServiceConfig *FindDBCfg(const std::string &title) const;

    // 查询「进程类型 → 可承载 Actor 类型列表」(配置优先, 缺失回退硬编码 switch)
    std::vector<EActorType> GetHostedActorTypes(EProcessType type) const;
    // 查询本进程可承载的 Actor 类型列表
    std::vector<EActorType> GetHostedActorTypes() const;

private:
    bool ParseServerSection(void *parser, void *root);
    bool ParseProcessSection(void *parser, void *root);
    bool ParseHostedActorsSection(void *parser, void *root);
    bool ParseClusterSection(void *parser, void *root);
    bool ParseRedisSection(void *parser, void *root);
    bool ParseDBSection(void *parser, void *root);

private:
    ServerXmlConfig m_stConfig;
    bool m_bLoaded = false;
};
