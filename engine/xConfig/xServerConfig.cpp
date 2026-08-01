#include "xServerConfig.h"
#include "xBase/xXMLParser.h"
#include "xLog/xLog.h"

namespace {
    // Safe stoi — returns default on empty or invalid input, never throws
    int safe_stoi(const std::string& s, int defaultVal = 0) {
        if (s.empty()) return defaultVal;
        try { return atoi(s.c_str()); }
        catch (...) { return defaultVal; }
    }
}

bool CServerConfig::Load(const std::string& strXmlPath) {
    xXMLParser parser;
    if (!parser.parseDoc(strXmlPath.c_str())) {
        XERR("[CServerConfig] Failed to parse config: %s", strXmlPath.c_str());
        return false;
    }

    xmlNodePtr root = parser.getRoot();
    if (!root) {
        XERR("[CServerConfig] Empty config file: %s", strXmlPath.c_str());
        return false;
    }

    if (!ParseServerSection(&parser, root)) {
        XWRN("[CServerConfig] Server section parsing had warnings");
    }

    if (!ParseProcessSection(&parser, root)) {
        XWRN("[CServerConfig] Process section not found — using defaults");
    }

    if (!ParseClusterSection(&parser, root)) {
        XWRN("[CServerConfig] Cluster fallback section not found — using defaults");
    }

    if (!ParseRedisSection(&parser, root)) {
        XWRN("[CServerConfig] No Redis pools configured");
    }

    if (!ParseDBSection(&parser, root)) {
        XWRN("[CServerConfig] No DB pools configured");
    }

    XLOG("[CServerConfig] Loaded: platform=%s grpc=%u process=%s type=%d cluster_peers=%zu redis_pools=%zu db_pools=%zu",
         m_stConfig.platform.c_str(), m_stConfig.gRPCPort,
         m_stConfig.process.name.c_str(), m_stConfig.process.type,
         m_stConfig.clusterFallback.peers.size(),
         m_stConfig.redisConfigs.size(), m_stConfig.dbConfigs.size());
    return true;
}

bool CServerConfig::ParseServerSection(void* p, void* r) {
    auto& parser = *static_cast<xXMLParser*>(p);
    auto root = static_cast<xmlNodePtr>(r);

    xmlNodePtr platformNode = parser.getChild(root, "platform");
    if (platformNode) {
        std::string inner;
        if (parser.InnerXml(platformNode, inner) && !inner.empty()) {
            m_stConfig.platform = inner;
        }
    }

    xmlNodePtr grpcNode = parser.getChild(root, "grpc");
    if (grpcNode) {
        std::string portStr;
        if (parser.getPropStr(grpcNode, "port", portStr)) {
            int p = safe_stoi(portStr);
            if (p > 0 && p <= 65535) {
                m_stConfig.gRPCPort = static_cast<uint16>(p);
            }
        }
    }

    return true;
}

bool CServerConfig::ParseRedisSection(void* p, void* r) {
    auto& parser = *static_cast<xXMLParser*>(p);
    auto root = static_cast<xmlNodePtr>(r);

    xmlNodePtr redisRoot = parser.getChild(root, "redis");
    if (!redisRoot) {
        return false;
    }

    for (xmlNodePtr node = parser.getChild(redisRoot); node; node = parser.getNext(node)) {
        if (node->type != XML_ELEMENT_NODE) continue;
        if (xmlStrcmp(node->name, BAD_CAST "pool") != 0) continue;

        ThridServiceConfig cfg;
        auto& redisCfg = cfg.redis;
        std::string val;

        if (parser.getPropStr(node, "title", val))         redisCfg.title = val;
        if (parser.getPropStr(node, "host", val))          redisCfg.host = val;
        if (parser.getPropStr(node, "port", val))          redisCfg.port = safe_stoi(val);
        if (parser.getPropStr(node, "max_conn", val))      redisCfg.max_conn = safe_stoi(val);
        if (parser.getPropStr(node, "conn_timeout", val))  redisCfg.conn_timeout = safe_stoi(val);
        if (parser.getPropStr(node, "retry_interval", val)) redisCfg.retry_interval = safe_stoi(val);
        if (parser.getPropStr(node, "user", val))          redisCfg.user = val;
        if (parser.getPropStr(node, "authkey", val))       redisCfg.authkey = val;

        if (redisCfg.title.empty()) {
            XWRN("[CServerConfig] Skipping Redis pool with empty title");
            continue;
        }

        XLOG("[CServerConfig] Redis pool: %s @ %s:%d (max_conn=%d)",
             redisCfg.title.c_str(), redisCfg.host.c_str(), redisCfg.port, redisCfg.max_conn);
        m_stConfig.redisConfigs.push_back(cfg);
    }

    return !m_stConfig.redisConfigs.empty();
}

bool CServerConfig::ParseDBSection(void* p, void* r) {
    auto& parser = *static_cast<xXMLParser*>(p);
    auto root = static_cast<xmlNodePtr>(r);

    xmlNodePtr dbRoot = parser.getChild(root, "db");
    if (!dbRoot) {
        return false;
    }

    for (xmlNodePtr node = parser.getChild(dbRoot); node; node = parser.getNext(node)) {
        if (node->type != XML_ELEMENT_NODE) continue;
        if (xmlStrcmp(node->name, BAD_CAST "pool") != 0) continue;

        ThridServiceConfig cfg;
        auto& dbCfg = cfg.mysql;
        std::string val;

        if (parser.getPropStr(node, "title", val))     dbCfg.title = val;
        if (parser.getPropStr(node, "host", val))      dbCfg.host = val;
        if (parser.getPropStr(node, "port", val))      dbCfg.port = safe_stoi(val);
        if (parser.getPropStr(node, "name", val))      dbCfg.dbName = val;
        if (parser.getPropStr(node, "user", val))      dbCfg.user = val;
        if (parser.getPropStr(node, "passwd", val))    dbCfg.passwd = val;
        if (parser.getPropStr(node, "poolCount", val)) dbCfg.poolCount = safe_stoi(val);

        if (dbCfg.title.empty()) {
            XWRN("[CServerConfig] Skipping DB pool with empty title");
            continue;
        }

        XLOG("[CServerConfig] DB pool: %s @ %s:%d/%s (pool=%d)",
             dbCfg.title.c_str(), dbCfg.host.c_str(), dbCfg.port,
             dbCfg.dbName.c_str(), dbCfg.poolCount);
        m_stConfig.dbConfigs.push_back(cfg);
    }

    return !m_stConfig.dbConfigs.empty();
}

// =============================================================================
// ParseProcessSection — 解析 <process> 配置
// =============================================================================
bool CServerConfig::ParseProcessSection(void* p, void* r) {
    auto& parser = *static_cast<xXMLParser*>(p);
    auto root = static_cast<xmlNodePtr>(r);

    xmlNodePtr procNode = parser.getChild(root, "process");
    if (!procNode) return false;

    std::string val;
    if (parser.getPropStr(procNode, "type", val))
        m_stConfig.process.type = safe_stoi(val, 100);
    if (parser.getPropStr(procNode, "name", val))
        m_stConfig.process.name = val;
    if (parser.getPropStr(procNode, "host", val))
        m_stConfig.process.host = val;

    XLOG("[CServerConfig] Process config: type=%d name=%s host=%s",
         m_stConfig.process.type, m_stConfig.process.name.c_str(),
         m_stConfig.process.host.c_str());
    return true;
}

// =============================================================================
// ParseClusterSection — 解析 <cluster> 回退配置
// =============================================================================
bool CServerConfig::ParseClusterSection(void* p, void* r) {
    auto& parser = *static_cast<xXMLParser*>(p);
    auto root = static_cast<xmlNodePtr>(r);

    xmlNodePtr clusterNode = parser.getChild(root, "cluster");
    if (!clusterNode) return false;

    // <local type="100" id="1" name="gate-dev-01"/>
    xmlNodePtr localNode = parser.getChild(clusterNode, "local");
    if (localNode) {
        std::string val;
        if (parser.getPropStr(localNode, "type", val))
            m_stConfig.clusterFallback.local.type = safe_stoi(val, 100);
        if (parser.getPropStr(localNode, "id", val))
            m_stConfig.clusterFallback.local.processId = safe_stoi(val, 1);
        if (parser.getPropStr(localNode, "name", val))
            m_stConfig.clusterFallback.local.name = val;
    }

    // <peer type="101" host="127.0.0.1" port="50052"/>
    for (xmlNodePtr peer = parser.getChild(clusterNode, "peer");
         peer; peer = parser.getNext(peer, "peer")) {
        ClusterPeerConfig pc;
        std::string val;
        if (parser.getPropStr(peer, "type", val)) pc.type = safe_stoi(val, 101);
        if (parser.getPropStr(peer, "host", val)) pc.host = val;
        if (parser.getPropStr(peer, "port", val)) pc.port = static_cast<uint16>(safe_stoi(val, 50051));
        m_stConfig.clusterFallback.peers.push_back(pc);
    }

    XLOG("[CServerConfig] Cluster fallback: local=%s (id=%lld) peers=%zu",
         m_stConfig.clusterFallback.local.name.c_str(),
         m_stConfig.clusterFallback.local.processId,
         m_stConfig.clusterFallback.peers.size());
    return true;
}

const ThridServiceConfig* CServerConfig::FindRedisCfg(const std::string& title) const {
    for (auto& cfg : m_stConfig.redisConfigs) {
        if (cfg.redis.title == title) return &cfg;
    }
    return nullptr;
}

const ThridServiceConfig* CServerConfig::FindDBCfg(const std::string& title) const {
    for (auto& cfg : m_stConfig.dbConfigs) {
        if (cfg.mysql.title == title) return &cfg;
    }
    return nullptr;
}
