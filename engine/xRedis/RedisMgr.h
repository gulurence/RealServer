#pragma once

#include "RedisCli.h"
#include "xBase/xSingleton.h"

#include <queue>
#include <mutex>
#include <condition_variable>
#include <hiredis/hiredis.h>


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
class CRedisPoolMgr : public xSingleton<CRedisPool>
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

