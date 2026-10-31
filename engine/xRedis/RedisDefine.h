/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: RedisDefine.h
      Purpose:   Redis 列表数据结构定义

      定义 ListData/ListDataMap 别名与 REDIS_LIST_OP_TYPE 枚举（POP/PUSH）
      注释说明旧 RedisConfigST/RedisConfigMap 已废弃，统一改用 ThridRedisConfig
  *****************************************************************/

#pragma once

#include "xBase/xDefine.h"
#include "xLog/xLog.h"

#include <list>
#include <map>
#include <string>

// RedisConfigST / RedisConfigMap 已废弃 — 统一使用 tService/tThridServiceConfig.h 中的
// ThridRedisConfig

typedef std::list<std::string> ListData;
typedef std::map<std::string, ListData> ListDataMap;

enum REDIS_LIST_OP_TYPE
{
    REDIS_LIST_OP_TYPE_POP = 0,
    REDIS_LIST_OP_TYPE_PUSH = 1,
    REDIS_LIST_OP_TYPE_MAX,
};
