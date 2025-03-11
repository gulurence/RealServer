#pragma once

#include "hiredis/hiredis.h"
#include "xBase/xDefine.h"
#include "xLog/xLog.h"

struct RedisConfigST
{
    uint32 redisid = 0;
    std::string title;
    std::string host = "127.0.0.1";
    int port = 6379;
    int max_conn = 20;           // 最大连接数‌:ml-citation{ref="1,6" data="citationList"}
    int conn_timeout = 3;        // 连接超时(秒)‌:ml-citation{ref="3" data="citationList"}
    int retry_interval = 1;      // 重试间隔(秒)‌:ml-citation{ref="8" data="citationList"}
    std::string user;
    std::string authkey;

    const RedisConfigST& operator=(const RedisConfigST& in) {
        if (this == &in) {
            return *this;
        }

        redisid = in.redisid;
        title = in.title;
        host = in.host;
        port = in.port;
        max_conn = in.max_conn;
        conn_timeout = in.conn_timeout;
        retry_interval = in.retry_interval;
        user = in.user;
        authkey = in.authkey;

        return *this;
    }
};

typedef std::list<std::string> ListData;
typedef std::map<std::string, ListData> ListDataMap;
typedef std::map<std::string, RedisConfigST> RedisConfigMap;

enum REDIS_LIST_OP_TYPE
{
    REDIS_LIST_OP_TYPE_POP = 0,
    REDIS_LIST_OP_TYPE_PUSH = 1,
    REDIS_LIST_OP_TYPE_MAX,
};

