#pragma once

#include "RedisCli.h"
#include "xBase/xSingleton.h"
#include "tService/tThridServiceConfig.h"

#include <queue>
#include <mutex>
#include <condition_variable>
#include <hiredis/hiredis.h>

// Forward declare redis++ types (async types temporarily disabled — needs libuv)
// namespace sw { namespace redis { class AsyncRedis; class RedisCluster; } }


class CRedisPool
{
public:
    CRedisPool(const ThridRedisConfig& cfg);
    ~CRedisPool();

    CRedisCli* GetConnection();
    void ReleaseConnection(CRedisCli* pConn);

private:
    CRedisCli* CreateConnection();
    bool Validate(CRedisCli* pConn);

private:
    std::queue<CRedisCli*> m_qPool_;
    std::mutex m_mLock_;
    std::condition_variable m_condCv_;
    ThridRedisConfig m_stConfig_;
};
typedef std::map<std::string, CRedisPool*> RedisPoolMap;

// 链接池管理
class CRedisPoolMgr : public xSingleton<CRedisPoolMgr>
{
public:
    CRedisPoolMgr(){}
    ~CRedisPoolMgr(){}

private:
    RedisPoolMap m_mapPool;

public:
    bool ConnectToRedis(const ThridRedisConfig& stConfig);
    CRedisCli* GetRedisCli(const std::string& strTitle);
};

#if 0 // Temporarily disabled: async redis requires libuv
typedef std::map<std::string, sw::redis::AsyncRedis*> AsyncRedisMap;
typedef std::map<std::string, sw::redis::RedisCluster*> RedisClusterMap;

class CRedisClusterMgr : public xSingleton<CRedisClusterMgr>
{
public:
    CRedisClusterMgr(){}
    ~CRedisClusterMgr(){}

private:
    RedisClusterMap m_mapRedis;

public:
    bool ConnectToCluster(const std::string &key, const std::string &url);
    sw::redis::RedisCluster* GetConnect(const std::string& key);
};

class CRedisAsyncMgr : public xSingleton<CRedisAsyncMgr>
{
public:
    CRedisAsyncMgr() {}
    ~CRedisAsyncMgr() {}

private:
    AsyncRedisMap m_mapAsyncRedisCluster;

public:
    bool ConnectToAsyncRedis(const std::string& key, const std::string& url);
    sw::redis::AsyncRedis* GetConnect(const std::string& key);
};
#endif // Temporarily disabled: async redis requires libuv
