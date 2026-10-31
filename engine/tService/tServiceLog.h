/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: tServiceLog.h
      Purpose:   日志服务声明

      声明继承 ThridService 的 tServiceLog，构造时传入 EST_Log 类型
      重写 Init 与 Release，内部复用 xLog(spdlog)
  *****************************************************************/

#pragma once

#include "tThridService.h"

// tServiceLog — 日志系统服务封装
// 内部使用 xLog (spdlog)
class tServiceLog : public ThridService
{
public:
    tServiceLog(int32 id, const std::string &name) : ThridService(EST_Log, id, name)
    {
    }

    virtual bool Init(const ThridServiceConfig &cfg) override;
    virtual void Release() override;
};
