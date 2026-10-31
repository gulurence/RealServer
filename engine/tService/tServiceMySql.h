/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: tServiceMySql.h
      Purpose:   MySQL 数据库服务声明

      声明继承 ThridService 的 tServiceMySql，类型为 EST_MySql
      Init 通过 COperatorMgr 连接池初始化，Release 关闭 DB 连接
  *****************************************************************/

#pragma once

#include "tThridService.h"

// tServiceMySql — MySQL 数据库服务封装
// 内部使用 COperatorMgr（连接池 + 异步操作管理器）
class tServiceMySql : public ThridService
{
public:
    tServiceMySql(int32 id, const std::string &name) : ThridService(EST_MySql, id, name)
    {
    }

    // 从 cfg.mysql 获取配置，初始化 COperatorMgr 连接池
    virtual bool Init(const ThridServiceConfig &cfg) override;

    // 关闭所有 DB 连接
    virtual void Release() override;
};
