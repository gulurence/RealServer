#include "RedisMgr.h"
#include "xBase/xXMLParser.h"
//#include "xSConnect/SStartConfig.h"


CRedisPool::CRedisPool(const RedisConfigST& cfg) : m_stConfig_(cfg) {
    for (int i = 0; i < cfg.max_conn / 2; ++i) { // 初始创建半数连接‌:ml-citation{ref="3" data="citationList"}
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
        m_condCv_.wait_for(lk, std::chrono::seconds(m_stConfig_.retry_interval)); // ‌:ml-citation{ref="8" data="citationList"}
    }

    auto* pConn = m_qPool_.front();
    m_qPool_.pop();

    if (!Validate(pConn)) {       // 失效连接重建‌:ml-citation{ref="2,3" data="citationList"}
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
        m_qPool_.push(pConn);  // 有效连接回池‌:ml-citation{ref="1" data="citationList"}
    }
    else {
        delete pConn;
        //redisFree(pConn);    // 失效连接销毁‌:ml-citation{ref="2" data="citationList"}
    }
    m_condCv_.notify_one();
}


bool CRedisPoolMgr::ConnectToRedis(const RedisConfigST& stConfig) {
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



#include <sw/redis++/redis++.h>
#include <sw/redis++/async_redis++.h>
#include <iostream>

int test_main() {
    try {
        sw::redis::AsyncRedis redis("tcp://127.0.0.1:6379");

        //redis.set("async_key", "async_value", [](const sw::redis::Error* err) {
        //    if (err) {
        //        std::cerr << "Error: " << err->what() << std::endl;
        //        return false;
        //    }
        //    else {
        //        std::cout << "Set successfully!" << std::endl;
        //        return true;
        //    }
        //});

        //redis.get("async_key", [](const sw::redis::Error* err, const std::optional<std::string>& val) {
        //    if (err) {
        //        std::cerr << "Error: " << err->what() << std::endl;
        //    }
        //    else if (val) {
        //        std::cout << "Get value: " << *val << std::endl;
        //    }
        //    else {
        //        std::cout << "Key not found" << std::endl;
        //    }
        //});

        // 等待异步操作完成
        //redis.wait();
    }
    catch (const sw::redis::Error& e) {
        std::cerr << "Redis error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}



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
