/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: xDatabaseDefine.h
      Purpose:   数据库结构定义（含已废弃配置）

      定义 DatabaseTableCol 结构（title/host），注释说明 DBConfigST 已废弃
      统一改用 tThridServiceConfig.h 中的 ThridMysqlConfig
  *****************************************************************/

#pragma once

#include "../xBase/xDefine.h"

// DBConfigST 已废弃 — 统一使用 tService/tThridServiceConfig.h 中的 ThridMysqlConfig

#pragma pack(1)
struct DatabaseTableCol
{
    std::string title;
    std::string host;

} /*__attribute__((packed))*/;
#pragma pack()
