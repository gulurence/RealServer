#include "xServerConfig.h"
#include "xBase/xXMLParser.h"
#include "xLog/xLog.h"

namespace
{
// Safe stoi — returns default on empty or invalid input, never throws
int32 safe_stoi(const std::string &s, int32 defaultVal = 0)
{
    if (s.empty())
    {
        return defaultVal;
    }
    try
    {
        return atoi(s.c_str());
    }
    catch (...)
    {
        return defaultVal;
    }
}
} // namespace

bool CServerConfig::Load(const std::string &strXmlPath)
{
    xXMLParser parser;
    if (!parser.parseDoc(strXmlPath.c_str()))
    {
        XERR("[CServerConfig] Failed to parse config: %s — process will run with DEFAULT identity "
             "(type=%d), verify startup log",
             strXmlPath.c_str(), (int32)m_stConfig.process.type);
        return false;
    }

    xmlNodePtr root = parser.getRoot();
    if (!root)
    {
        XERR("[CServerConfig] Empty config file: %s", strXmlPath.c_str());
        return false;
    }

    if (!ParseServerSection(&parser, root))
    {
        XWRN("[CServerConfig] Server section parsing had warnings");
    }

    if (!ParseProcessSection(&parser, root))
    {
        XWRN("[CServerConfig] Process section not found — using defaults");
    }

    if (!ParseHostedActorsSection(&parser, root))
    {
        XWRN("[CServerConfig] hosted-actors section not found — falling back to hardcoded map");
    }

    if (!ParseClusterSection(&parser, root))
    {
        XWRN("[CServerConfig] Cluster fallback section not found — using defaults");
    }

    if (!ParseRedisSection(&parser, root))
    {
        XWRN("[CServerConfig] No Redis pools configured");
    }

    if (!ParseDBSection(&parser, root))
    {
        XWRN("[CServerConfig] No DB pools configured");
    }

    XLOG("[CServerConfig] Loaded: platform=%s rpc=%u process=%s type=%d hosted_entries=%zu "
         "cluster_peers=%zu redis_pools=%zu db_pools=%zu",
         m_stConfig.platform.c_str(), m_stConfig.rpcPort, m_stConfig.process.name.c_str(),
         (int32)m_stConfig.process.type, m_stConfig.hostedActors.size(),
         m_stConfig.clusterFallback.peers.size(), m_stConfig.redisConfigs.size(),
         m_stConfig.dbConfigs.size());
    m_bLoaded = true;
    return true;
}

bool CServerConfig::ParseServerSection(void *p, void *r)
{
    auto &parser = *static_cast<xXMLParser *>(p);
    auto root = static_cast<xmlNodePtr>(r);

    xmlNodePtr platformNode = parser.getChild(root, "platform");
    if (platformNode)
    {
        std::string inner;
        if (parser.InnerXml(platformNode, inner) && !inner.empty())
        {
            m_stConfig.platform = inner;
        }
    }

    xmlNodePtr rpcNode = parser.getChild(root, "rpc");
    if (rpcNode)
    {
        std::string portStr;
        if (parser.getPropStr(rpcNode, "port", portStr))
        {
            int32 p = safe_stoi(portStr);
            if (p > 0 && p <= 65535)
            {
                m_stConfig.rpcPort = static_cast<uint16>(p);
            }
        }
    }

    return true;
}

bool CServerConfig::ParseRedisSection(void *p, void *r)
{
    auto &parser = *static_cast<xXMLParser *>(p);
    auto root = static_cast<xmlNodePtr>(r);

    xmlNodePtr redisRoot = parser.getChild(root, "redis");
    if (!redisRoot)
    {
        return false;
    }

    for (xmlNodePtr node = parser.getChild(redisRoot); node; node = parser.getNext(node))
    {
        if (node->type != XML_ELEMENT_NODE)
        {
            continue;
        }
        if (xmlStrcmp(node->name, BAD_CAST "pool") != 0)
        {
            continue;
        }

        ThridServiceConfig cfg;
        auto &redisCfg = cfg.redis;
        std::string val;

        if (parser.getPropStr(node, "title", val))
        {
            redisCfg.title = val;
        }
        if (parser.getPropStr(node, "host", val))
        {
            redisCfg.host = val;
        }
        if (parser.getPropStr(node, "port", val))
        {
            redisCfg.port = safe_stoi(val);
        }
        if (parser.getPropStr(node, "max_conn", val))
        {
            redisCfg.max_conn = safe_stoi(val);
        }
        if (parser.getPropStr(node, "conn_timeout", val))
        {
            redisCfg.conn_timeout = safe_stoi(val);
        }
        if (parser.getPropStr(node, "retry_interval", val))
        {
            redisCfg.retry_interval = safe_stoi(val);
        }
        if (parser.getPropStr(node, "user", val))
        {
            redisCfg.user = val;
        }
        if (parser.getPropStr(node, "authkey", val))
        {
            redisCfg.authkey = val;
        }

        if (redisCfg.title.empty())
        {
            XWRN("[CServerConfig] Skipping Redis pool with empty title");
            continue;
        }

        XLOG("[CServerConfig] Redis pool: %s @ %s:%d (max_conn=%d)", redisCfg.title.c_str(),
             redisCfg.host.c_str(), redisCfg.port, redisCfg.max_conn);
        m_stConfig.redisConfigs.push_back(cfg);
    }

    return !m_stConfig.redisConfigs.empty();
}

bool CServerConfig::ParseDBSection(void *p, void *r)
{
    auto &parser = *static_cast<xXMLParser *>(p);
    auto root = static_cast<xmlNodePtr>(r);

    xmlNodePtr dbRoot = parser.getChild(root, "db");
    if (!dbRoot)
    {
        return false;
    }

    for (xmlNodePtr node = parser.getChild(dbRoot); node; node = parser.getNext(node))
    {
        if (node->type != XML_ELEMENT_NODE)
        {
            continue;
        }
        if (xmlStrcmp(node->name, BAD_CAST "pool") != 0)
        {
            continue;
        }

        ThridServiceConfig cfg;
        auto &dbCfg = cfg.mysql;
        std::string val;

        if (parser.getPropStr(node, "title", val))
        {
            dbCfg.title = val;
        }
        if (parser.getPropStr(node, "host", val))
        {
            dbCfg.host = val;
        }
        if (parser.getPropStr(node, "port", val))
        {
            dbCfg.port = safe_stoi(val);
        }
        if (parser.getPropStr(node, "name", val))
        {
            dbCfg.dbName = val;
        }
        if (parser.getPropStr(node, "user", val))
        {
            dbCfg.user = val;
        }
        if (parser.getPropStr(node, "passwd", val))
        {
            dbCfg.passwd = val;
        }
        if (parser.getPropStr(node, "poolCount", val))
        {
            dbCfg.poolCount = safe_stoi(val);
        }

        if (dbCfg.title.empty())
        {
            XWRN("[CServerConfig] Skipping DB pool with empty title");
            continue;
        }

        XLOG("[CServerConfig] DB pool: %s @ %s:%d/%s (pool=%d)", dbCfg.title.c_str(),
             dbCfg.host.c_str(), dbCfg.port, dbCfg.dbName.c_str(), dbCfg.poolCount);
        m_stConfig.dbConfigs.push_back(cfg);
    }

    return !m_stConfig.dbConfigs.empty();
}

// =============================================================================
// ParseProcessSection — 解析 <process> 配置
// =============================================================================
bool CServerConfig::ParseProcessSection(void *p, void *r)
{
    auto &parser = *static_cast<xXMLParser *>(p);
    auto root = static_cast<xmlNodePtr>(r);

    xmlNodePtr procNode = parser.getChild(root, "process");
    if (!procNode)
    {
        return false;
    }

    std::string val;
    if (parser.getPropStr(procNode, "type", val))
    {
        EProcessType t = (EProcessType)safe_stoi(val, 100);
        if (!IsValidProcessType(t))
        {
            XWRN("[CServerConfig] process type=%d invalid — falling back to Gate(100)", (int32)t);
            t = EProcessType_PGate;
        }
        m_stConfig.process.type = t;
    }
    if (parser.getPropStr(procNode, "name", val))
    {
        m_stConfig.process.name = val;
    }
    if (parser.getPropStr(procNode, "host", val))
    {
        m_stConfig.process.host = val;
    }
    if (parser.getPropStr(procNode, "port", val))
    {
        int32 p = safe_stoi(val, 10000);
        if (p > 0 && p <= 65535)
        {
            m_stConfig.process.listenPort = static_cast<uint16>(p);
        }
    }
    if (parser.getPropStr(procNode, "machine", val))
    {
        m_stConfig.process.machineId = (uint32)safe_stoi(val, 0);
    }

    // <hosted-actors> 嵌套声明 (可选) — 本进程可承载的 Actor 类型, 覆盖顶层 <hosted-actors> 表
    xmlNodePtr hostedNode = parser.getChild(procNode, "hosted-actors");
    if (hostedNode)
    {
        for (xmlNodePtr actorNode = parser.getChild(hostedNode, "actor"); actorNode;
             actorNode = parser.getNext(actorNode, "actor"))
        {
            std::string actorVal;
            if (parser.getPropStr(actorNode, "type", actorVal))
            {
                EActorType t = (EActorType)safe_stoi(actorVal, 0);
                if (t != EActorType_None && ActorTypeToProcessType(t) != EProcessType_None)
                {
                    m_stConfig.process.hostedActors.push_back(t);
                }
                else if (t != EActorType_None)
                {
                    XWRN("[CServerConfig] nested hosted-actors: invalid actor type=%d ignored",
                         (int32)t);
                }
            }
        }
        XLOG("[CServerConfig] Process nested hosted-actors: %zu actor type(s)",
             m_stConfig.process.hostedActors.size());
    }

    XLOG("[CServerConfig] Process config: type=%d name=%s host=%s listenPort=%u",
         (int32)m_stConfig.process.type, m_stConfig.process.name.c_str(),
         m_stConfig.process.host.c_str(), m_stConfig.process.listenPort);
    return true;
}

// =============================================================================
// ParseHostedActorsSection — 解析顶层 <hosted-actors> 声明表
//
// schema:
//   <hosted-actors>
//       <process type="100"><actor type="100"/></process>
//       <process type="102"><actor type="102"/><actor type="103"/>...</process>
//   </hosted-actors>
//
// 该表描述「进程类型 → 可承载 Actor 类型列表」, 用于覆盖 xProcessDefine.h
// 硬编码 GetHostedActorTypes; 缺失或某类型未声明时回退硬编码 switch。
// =============================================================================
bool CServerConfig::ParseHostedActorsSection(void *p, void *r)
{
    auto &parser = *static_cast<xXMLParser *>(p);
    auto root = static_cast<xmlNodePtr>(r);

    xmlNodePtr hostedRoot = parser.getChild(root, "hosted-actors");
    if (!hostedRoot)
    {
        return false;
    }

    for (xmlNodePtr procNode = parser.getChild(hostedRoot, "process"); procNode;
         procNode = parser.getNext(procNode, "process"))
    {
        HostedActorsConfig entry;
        std::string val;
        EProcessType pt = EProcessType_None;
        if (parser.getPropStr(procNode, "type", val))
        {
            pt = (EProcessType)safe_stoi(val, 0);
        }
        if (pt == EProcessType_None || !IsValidProcessType(pt))
        {
            XWRN("[CServerConfig] Skipping hosted-actors process entry with invalid type=%d",
                 (int32)pt);
            continue;
        }
        entry.processType = pt;

        for (xmlNodePtr actorNode = parser.getChild(procNode, "actor"); actorNode;
             actorNode = parser.getNext(actorNode, "actor"))
        {
            std::string actorVal;
            if (parser.getPropStr(actorNode, "type", actorVal))
            {
                EActorType t = (EActorType)safe_stoi(actorVal, 0);
                if (t != EActorType_None && ActorTypeToProcessType(t) != EProcessType_None)
                {
                    entry.actorTypes.push_back(t);
                }
                else if (t != EActorType_None)
                {
                    XWRN("[CServerConfig] hosted-actors: invalid actor type=%d ignored", (int32)t);
                }
            }
        }

        XLOG("[CServerConfig] hosted-actors: process type=%d hosts %zu actor type(s)",
             (int32)entry.processType, entry.actorTypes.size());
        m_stConfig.hostedActors.push_back(entry);
    }

    return !m_stConfig.hostedActors.empty();
}

// =============================================================================
// GetHostedActorTypes — 查询「进程类型 → 可承载 Actor 类型列表」
//
// 优先级: <process><hosted-actors> 嵌套(本进程) > 顶层 <hosted-actors> 表
//          > xProcessDefine.h 硬编码 GetHostedActorTypes (回退)
// =============================================================================
std::vector<EActorType> CServerConfig::GetHostedActorTypes(EProcessType type) const
{
    // 1) 本进程 <process> 段嵌套声明 (仅当查询类型 == 本进程类型且非空)
    if (type == m_stConfig.process.type && !m_stConfig.process.hostedActors.empty())
    {
        return m_stConfig.process.hostedActors;
    }
    // 2) 顶层 <hosted-actors> 声明表
    for (const auto &entry : m_stConfig.hostedActors)
    {
        if (entry.processType == type && !entry.actorTypes.empty())
        {
            return entry.actorTypes;
        }
    }
    // 3) 回退硬编码 switch
    XWRN("[CServerConfig] GetHostedActorTypes: type=%d not declared in config, falling back to "
         "hardcoded map",
         (int32)type);
    return ::GetHostedActorTypes(type);
}

std::vector<EActorType> CServerConfig::GetHostedActorTypes() const
{
    return GetHostedActorTypes(m_stConfig.process.type);
}

// =============================================================================
// ParseClusterSection — 解析 <cluster> 回退配置
// =============================================================================
bool CServerConfig::ParseClusterSection(void *p, void *r)
{
    auto &parser = *static_cast<xXMLParser *>(p);
    auto root = static_cast<xmlNodePtr>(r);

    xmlNodePtr clusterNode = parser.getChild(root, "cluster");
    if (!clusterNode)
    {
        return false;
    }

    // <auth secret="RealServerAuthSecret"/> — 验证 Key 派生密钥
    xmlNodePtr authNode = parser.getChild(clusterNode, "auth");
    if (authNode)
    {
        std::string secretVal; // 独立字符串, 规避 getPropStr 追加 bug
        if (parser.getPropStr(authNode, "secret", secretVal) && !secretVal.empty())
        {
            m_stConfig.authSecret = secretVal;
        }
    }

    // <local type="100" id="1" name="gate-dev-01"/>
    xmlNodePtr localNode = parser.getChild(clusterNode, "local");
    if (localNode)
    {
        std::string val;
        if (parser.getPropStr(localNode, "type", val))
        {
            m_stConfig.clusterFallback.local.type = (EProcessType)safe_stoi(val, 100);
        }
        if (parser.getPropStr(localNode, "id", val))
        {
            m_stConfig.clusterFallback.local.processId = safe_stoi(val, 1);
        }
        if (parser.getPropStr(localNode, "name", val))
        {
            m_stConfig.clusterFallback.local.name = val;
        }
    }

    // <peer type="101" host="127.0.0.1" port="50052"/>
    for (xmlNodePtr peer = parser.getChild(clusterNode, "peer"); peer;
         peer = parser.getNext(peer, "peer"))
    {
        ClusterPeerConfig pc;
        std::string val;
        if (parser.getPropStr(peer, "type", val))
        {
            pc.type = (EProcessType)safe_stoi(val, 101);
        }
        if (parser.getPropStr(peer, "host", val))
        {
            pc.host = val;
        }
        if (parser.getPropStr(peer, "port", val))
        {
            pc.port = static_cast<uint16>(safe_stoi(val, 50051));
        }
        m_stConfig.clusterFallback.peers.push_back(pc);
    }

    XLOG("[CServerConfig] Cluster fallback: local=%s (id=%lld) peers=%zu auth_secret=%s",
         m_stConfig.clusterFallback.local.name.c_str(), m_stConfig.clusterFallback.local.processId,
         m_stConfig.clusterFallback.peers.size(),
         m_stConfig.authSecret.empty() ? "(default)" : m_stConfig.authSecret.c_str());
    return true;
}

const ThridServiceConfig *CServerConfig::FindRedisCfg(const std::string &title) const
{
    for (auto &cfg : m_stConfig.redisConfigs)
    {
        if (cfg.redis.title == title)
        {
            return &cfg;
        }
    }
    return nullptr;
}

const ThridServiceConfig *CServerConfig::FindDBCfg(const std::string &title) const
{
    for (auto &cfg : m_stConfig.dbConfigs)
    {
        if (cfg.mysql.title == title)
        {
            return &cfg;
        }
    }
    return nullptr;
}
