#pragma once

#include "hiredis/hiredis.h"
#include "xBase/xDefine.h"
#include "xLog/xLog.h"

// RedisConfigST / RedisConfigMap 已废弃 — 统一使用 tService/tThridServiceConfig.h 中的 ThridRedisConfig

typedef std::list<std::string> ListData;
typedef std::map<std::string, ListData> ListDataMap;

enum REDIS_LIST_OP_TYPE
{
    REDIS_LIST_OP_TYPE_POP = 0,
    REDIS_LIST_OP_TYPE_PUSH = 1,
    REDIS_LIST_OP_TYPE_MAX,
};
