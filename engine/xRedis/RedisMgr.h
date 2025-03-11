#pragma once

#include "RedisCli.h"
#include "xBase/xSingleton.h"

#include <queue>
#include <mutex>
#include <condition_variable>
#include <hiredis/hiredis.h>


class CRedisPool : public xSingleton<CRedisPool>
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
    void ReleaseConnection(CRedisCli* conn);

private:
    CRedisCli* CreateConnection();
    bool Validate(CRedisCli* conn);

    std::queue<CRedisCli*> pool_;
    std::mutex mtx_;
    std::condition_variable cv_;
    RedisConfigST config_;
};

