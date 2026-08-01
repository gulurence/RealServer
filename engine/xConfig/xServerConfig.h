#pragma once

#include "xBase/xSingleton.h"
#include "xCluster/xProcessDefine.h"
#include "tService/tThridServiceConfig.h"
#include <vector>
#include <string>

// =============================================================================
// 进程配置 — 标识本进程的角色和身份
// =============================================================================
struct ProcessXmlConfig {
    int32       type = 100;                 // EProcessType / ActorType
    std::string name = "default";           // 进程名称 (e.g. "gate-01")
    std::string host = "0.0.0.0";          // 监听地址
};

// =============================================================================
// 集群本地回退配置 — 当 Redis 不可用时使用
// =============================================================================
struct ClusterLocalConfig {
    int32       type = 100;
    int64       processId = 1;
    std::string name = "local";
};

struct ClusterPeerConfig {
    int32       type = 101;
    std::string host = "127.0.0.1";
    uint16      port = 50051;
};

struct ClusterFallbackConfig {
    ClusterLocalConfig              local;
    std::vector<ClusterPeerConfig>  peers;
};

// =============================================================================
// 服务发现连接池配置
// =============================================================================
struct DiscoveryRedisConfig {
    std::string poolTitle = "cluster_discovery";  // Redis 池名称
};

// Holds all server configuration parsed from server.xml at startup.
// Designed as a POCO (Plain Old Config Object) consumed by main.cpp.
struct ServerXmlConfig {
    uint16    gRPCPort = 50051;
    std::string platform = "dev";           // dev / test / prod
    std::vector<ThridServiceConfig> redisConfigs; // Redis pool configs
    std::vector<ThridServiceConfig> dbConfigs;    // DB pool configs

    // 进程标识配置
    ProcessXmlConfig     process;
    // 集群回退配置 (Redis 不可用时)
    ClusterFallbackConfig clusterFallback;
    // 服务发现 Redis 池
    DiscoveryRedisConfig discoveryRedis;
};

// CServerConfig — singleton configuration loader.
class CServerConfig : public xSingleton<CServerConfig>
{
public:
    bool Load(const std::string& strXmlPath);

    const ServerXmlConfig& GetConfig() const { return m_stConfig; }
    ServerXmlConfig& MutableConfig() { return m_stConfig; }

    const ThridServiceConfig* FindRedisCfg(const std::string& title) const;
    const ThridServiceConfig* FindDBCfg(const std::string& title) const;

private:
    bool ParseServerSection(void* parser, void* root);
    bool ParseProcessSection(void* parser, void* root);
    bool ParseClusterSection(void* parser, void* root);
    bool ParseRedisSection(void* parser, void* root);
    bool ParseDBSection(void* parser, void* root);

private:
    ServerXmlConfig m_stConfig;
};
