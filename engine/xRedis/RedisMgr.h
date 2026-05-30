#pragma once

#include "RedisCli.h"
#include "xBase/xSingleton.h"

#include <queue>
#include <mutex>
#include <condition_variable>
#include <hiredis/hiredis.h>

#include "sw/redis++/async_redis.h"
#include "sw/redis++/redis_cluster.h"


class CRedisPool
{
public:
    struct Config
    {
        std::string host = "127.0.0.1";
        int port = 6379;
        int max_conn = 20;           // 最大连接数‌:ml-citation{ref="1,6" data="citationList"}
        int conn_timeout = 3;        // 连接超时(秒)‌:ml-citation{ref="3" data="citationList"}
        int retry_interval = 1;      // 重试间隔(秒)‌:ml-citation{ref="8" data="citationList"}
    };

    CRedisPool(const RedisConfigST& cfg);
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
    RedisConfigST m_stConfig_;
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
    bool ConnectToRedis(const RedisConfigST& stConfig);
    CRedisCli* GetRedisCli(const std::string& strTitle);
};

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
    // "tcp://127.0.0.1:7000"
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
    // "tcp://127.0.0.1:7000"
    bool ConnectToAsyncRedis(const std::string& key, const std::string& url);
    sw::redis::AsyncRedis* GetConnect(const std::string& key);
};
