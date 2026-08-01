#pragma once

#include "tThridService.h"

// tServiceMySql — MySQL 数据库服务封装
// 内部使用 COperatorMgr（连接池 + 异步操作管理器）
class tServiceMySql : public ThridService
{
public:
    tServiceMySql(int32 id, const std::string& name)
        : ThridService(EST_MySql, id, name) {}

    // 从 cfg.mysql 获取配置，初始化 COperatorMgr 连接池
    virtual bool Init(const ThridServiceConfig& cfg) override;

    // 关闭所有 DB 连接
    virtual void Release() override;
};
