#pragma once

#include "../xBase/xDefine.h"

// DBConfigST 已废弃 — 统一使用 tService/tThridServiceConfig.h 中的 ThridMysqlConfig

#pragma pack(1)
struct DatabaseTableCol {
    std::string title;
    std::string host;

}/*__attribute__((packed))*/;
#pragma pack()
