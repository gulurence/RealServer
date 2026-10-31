/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: tThridServer.cpp
      Purpose:   第三方服务基类实现

      实现 ThridService 构造函数与析构，记录服务类型、ID、名称及创建时间
      构造与析构通过 xLog 输出生命周期日志
  *****************************************************************/

#include "tThridService.h"
#include "xBase/xTime.h"
#include "xLog/xLog.h"

ThridService::ThridService(EThridServiceType enType, int32 id, const std::string &name)
    : m_enType(enType), m_i32ID(id), m_strName(name),
      m_i64CreateTime(static_cast<int64>(xTime::getCurSec()))
{
    XLOG("[ThridService] Created: type=%d name=%s", (int32)enType, name.c_str());
}

ThridService::~ThridService()
{
    XLOG("[ThridService] Destroyed: type=%d name=%s", (int32)m_enType, m_strName.c_str());
}
