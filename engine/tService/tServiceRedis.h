/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: tServiceRedis.h
      Purpose:   Redis 缓存服务声明

      声明继承 ThridService 的 tServiceRedis（EST_Redis）
      Init 通过 CRedisPoolMgr 连接池初始化，Release 关闭 Redis 连接
  *****************************************************************/

#pragma once

#include "tThridService.h"

// tServiceRedis — Redis 缓存服务封装
// 内部使用 CRedisPoolMgr（连接池管理器）
class tServiceRedis : public ThridService
{
public:
    tServiceRedis(int32 id, const std::string &name) : ThridService(EST_Redis, id, name)
    {
    }

    // 从 cfg.redis 获取配置，初始化 CRedisPoolMgr 连接池
    virtual bool Init(const ThridServiceConfig &cfg) override;

    // 关闭所有 Redis 连接
    virtual void Release() override;
};
