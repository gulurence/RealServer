#pragma once

#include "hiredis/hiredis.h"
#include "xBase/xDefine.h"


struct RedisConfig
{
    uint32 redisid = 0;
    std::string title;
    std::string ip;
    uint32 port = 0;
    std::string user;
    std::string authkey;

    const RedisConfig& operator=(const RedisConfig& in) {
        if (this == &in) {
            return *this;
        }

        redisid = in.redisid;
        title = in.title;
        ip = in.ip;
        port = in.port;
        user = in.user;
        authkey = in.authkey;

        return *this;
    }
};

typedef std::list<std::string> ListData;
typedef std::map<std::string, ListData> ListDataMap;
typedef std::map<std::string, RedisConfig> RedisConfigMap;

enum REDIS_LIST_OP_TYPE
{
    REDIS_LIST_OP_TYPE_POP = 0,
    REDIS_LIST_OP_TYPE_PUSH = 1,
    REDIS_LIST_OP_TYPE_MAX,
};
