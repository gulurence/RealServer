#pragma once

#include "tThridService.h"

// tServiceLog — 日志系统服务封装
// 内部使用 xLog (spdlog)
class tServiceLog : public ThridService
{
public:
    tServiceLog(int32 id, const std::string& name)
        : ThridService(EST_Log, id, name) {}

    virtual bool Init(const ThridServiceConfig& cfg) override;
    virtual void Release() override;
};
