/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: RedisMgr.h
      Purpose:   Redis 连接管理器（单例）

      定义 CRedisPoolMgr 单例，按 title 维护 CRedisCli 共享句柄映射
      ConnectToRedis 建立连接，GetRedisCli 返回线程安全的同一 RedisCluster 句柄供各模块复用
  *****************************************************************/

#pragma once

#include "RedisCli.h"
#include "xBase/xSingleton.h"
#include "tService/tThridServiceConfig.h"

#include <map>
#include <string>

// CRedisPoolMgr — Redis 连接管理 (每 title 一个共享 CRedisCli)
//
// RedisCluster 自带连接池且线程安全, 无需再维护 queue<CRedisCli*> + mutex/cv
// 的「取出/归还」连接池 (旧实现存在取出不归还致池耗尽的隐患)。改为:
//   ConnectToRedis(title) → 构造一个共享 CRedisCli (内部持 RedisCluster)
//   GetRedisCli(title)    → 返回持久化、线程安全的同一句柄
// 与现有用法契合: CTraceManager/CPlayerActorIndex/CServiceDiscovery 长期持有
// m_pCli, xRedisRecord/luacore 按次取也拿到同一句柄即可。
class CRedisPoolMgr : public xSingleton<CRedisPoolMgr>
{
public:
    CRedisPoolMgr()
    {
    }
    ~CRedisPoolMgr()
    {
        for (auto &kv : m_mapCli)
        {
            delete kv.second;
        }
        m_mapCli.clear();
    }

private:
    std::map<std::string, CRedisCli *> m_mapCli;

public:
    bool ConnectToRedis(const ThridRedisConfig &stConfig);
    CRedisCli *GetRedisCli(const std::string &strTitle);
};
