#include "RedisMgr.h"
#include "xBase/xXMLParser.h"
// Temporarily disabled: async redis requires libuv which is not available
// #include "sw/redis++/async_redis.h"
// #include "sw/redis++/redis_cluster.h"


CRedisPool::CRedisPool(const ThridRedisConfig& cfg) : m_stConfig_(cfg) {
    for (int i = 0; i < cfg.max_conn / 2; ++i) { // 初始创建半数连接
        CreateConnection();
    }
}

CRedisPool::~CRedisPool() {
    while (m_qPool_.empty()) {
        auto* pConn = m_qPool_.front();
        m_qPool_.pop();
        delete pConn;
    }
}

CRedisCli* CRedisPool::CreateConnection() {
    CRedisCli* pCli = new CRedisCli();
    if (pCli->ConnectDB(m_stConfig_.host.c_str(), m_stConfig_.port) != 0) {
        XERR("CRedisPool::CreateConnection ConnectDB error [title:%s,host:%s,port:%d] !!!", m_stConfig_.title.c_str(), m_stConfig_.host.c_str(), m_stConfig_.port);
        delete pCli;
        return nullptr;
    }
    return pCli;
}

CRedisCli* CRedisPool::GetConnection() {
    std::unique_lock<std::mutex> lk(m_mLock_);

    while (m_qPool_.empty()) {
        if (m_qPool_.size() < m_stConfig_.max_conn) {
            auto* pConn = CreateConnection();
            if (pConn) m_qPool_.push(pConn);
        }
        m_condCv_.wait_for(lk, std::chrono::seconds(m_stConfig_.retry_interval));
    }

    auto* pConn = m_qPool_.front();
    m_qPool_.pop();

    if (!Validate(pConn)) {       // 失效连接重建
        delete pConn;
        //redisFree(pConn);
        pConn = CreateConnection();
    }
    return pConn;
}

bool CRedisPool::Validate(CRedisCli* pConn) {
    if (!pConn || !pConn->Validate()) {
        XERR("CRedisPool::Validate Connect is error [title:%s,host:%s,port:%d] !!!", m_stConfig_.title.c_str(), m_stConfig_.host.c_str(), m_stConfig_.port);
        return false;
    }
    return true;
}

void CRedisPool::ReleaseConnection(CRedisCli* pConn) {
    std::lock_guard<std::mutex> lk(m_mLock_);

    if (Validate(pConn)) {
        m_qPool_.push(pConn);  // 有效连接回池
    }
    else {
        delete pConn;
        //redisFree(pConn);    // 失效连接销毁
    }
    m_condCv_.notify_one();
}


bool CRedisPoolMgr::ConnectToRedis(const ThridRedisConfig& stConfig) {
    if (stConfig.title.empty()) {
        XERR("CRedisPoolMgr::ConnectToRedis titile is empty !!!");
        return false;
    }
    auto it = m_mapPool.find(stConfig.title);
    if (it != m_mapPool.end()) {
        XERR("CRedisPoolMgr::ConnectToRedis have same titile pool !!!");
        return false;
    }
    m_mapPool[stConfig.title] = new CRedisPool(stConfig);
    return true;
}

CRedisCli* CRedisPoolMgr::GetRedisCli(const std::string& strTitle) {
    auto it = m_mapPool.find(strTitle);
    if (it == m_mapPool.end()) {
        XERR("CRedisPoolMgr::GetRedisCli not find titile pool connect [%s] !!!", strTitle.c_str());
        return nullptr;
    }
    CRedisPool* pPool = it->second;
    return pPool->GetConnection();
}





#if 0 // Temporarily disabled: async redis requires libuv
bool CRedisClusterMgr::ConnectToCluster(const std::string& key, const std::string& url) {
    if (key.empty()) {
        XERR("CRedisClusterMgr::ConnectToCluster key.empty()");
        return false;
    }
    try
    {
        sw::redis::RedisCluster*cluster = new sw::redis::RedisCluster(url);
        m_mapRedis[key] = cluster;
    }
    catch (const sw::redis::Error& e)
    {
        XERR("CRedisClusterMgr::ConnectToCluster key.empty() [%s]", e.what());
        return false;
    }
    return true;
}

sw::redis::RedisCluster* CRedisClusterMgr::GetConnect(const std::string& key) {
    auto it = m_mapRedis.find(key);
    if (it == m_mapRedis.end()) {
        return nullptr;
    }
    return it->second;
}


bool CRedisAsyncMgr::ConnectToAsyncRedis(const std::string& key, const std::string& url) {
    if (key.empty()) {
        XERR("CRedisClusterMgr::ConnectToCluster key.empty()");
        return false;
    }
    try
    {
        sw::redis::AsyncRedis* cluster = new sw::redis::AsyncRedis(url);
        m_mapAsyncRedisCluster[key] = cluster;
    }
    catch (const sw::redis::Error& e)
    {
        XERR("CRedisClusterMgr::ConnectToCluster key.empty() [%s]", e.what());
        return false;
    }
    return true;
}

sw::redis::AsyncRedis* CRedisAsyncMgr::GetConnect(const std::string& key) {
    auto it = m_mapAsyncRedisCluster.find(key);
    if (it == m_mapAsyncRedisCluster.end()) {
        return nullptr;
    }
    return it->second;
}
#endif // Temporarily disabled: async redis requires libuv
