/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: RedisMgr.cpp
      Purpose:   Redis 连接管理器实现

      实现 CRedisPoolMgr::ConnectToRedis 按 title 构造共享 CRedisCli 并校验重复连接
      GetRedisCli 按 title 返回持久化句柄，未找到时记录错误并返回 nullptr
  *****************************************************************/

#include "RedisMgr.h"
#include "xBase/xXMLParser.h"

bool CRedisPoolMgr::ConnectToRedis(const ThridRedisConfig &stConfig)
{
    if (stConfig.title.empty())
    {
        XERR("CRedisPoolMgr::ConnectToRedis titile is empty !!!");
        return false;
    }
    if (m_mapCli.find(stConfig.title) != m_mapCli.end())
    {
        XERR("CRedisPoolMgr::ConnectToRedis have same titile pool [%s] !!!",
             stConfig.title.c_str());
        return false;
    }

    // 构造共享句柄: 一个 title 一个 CRedisCli (内部 RedisCluster 自带连接池)
    CRedisCli *pCli = new CRedisCli();
    if (pCli->ConnectDB(stConfig) != REDIS_OK)
    {
        XERR("CRedisPoolMgr::ConnectToRedis ConnectDB error [title:%s,host:%s,port:%d] !!!",
             stConfig.title.c_str(), stConfig.host.c_str(), stConfig.port);
        delete pCli;
        return false;
    }
    m_mapCli[stConfig.title] = pCli;
    return true;
}

CRedisCli *CRedisPoolMgr::GetRedisCli(const std::string &strTitle)
{
    auto it = m_mapCli.find(strTitle);
    if (it == m_mapCli.end())
    {
        XERR("CRedisPoolMgr::GetRedisCli not find titile pool connect [%s] !!!", strTitle.c_str());
        return nullptr;
    }
    return it->second;
}
