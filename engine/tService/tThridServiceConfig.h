#pragma once

#include "xBase/xDefine.h"
#include <string>
#include <set>

// =============================================================================
// EThridServiceType — 第三方服务类型枚举
// =============================================================================
enum EThridServiceType {
    EST_None        = 0,
    EST_MySql       = 1,   // MySQL 数据库
    EST_Redis       = 2,   // Redis 缓存
    EST_Log         = 3,   // 日志系统
    EST_Xml         = 4,   // XML 配置/解析
    EST_PlatformMgr = 5,   // 平台管理 (链路追踪/上报)
    EST_Max
};

// =============================================================================
// 每种类型的专用配置结构体
// =============================================================================

// MySQL 配置
struct ThridMysqlConfig {
    std::string title;
    std::string host     = "127.0.0.1";
    int         port     = 3306;
    std::string dbName;
    std::string user;
    std::string passwd;
    int         poolCount = 10;
};

// Redis 配置
struct ThridRedisConfig {
    std::string title;
    std::string host     = "127.0.0.1";
    int         port     = 6379;
    int         max_conn = 20;           // 最大连接数
    int         conn_timeout = 3;        // 连接超时(秒)
    int         retry_interval = 1;      // 重试间隔(秒)
    std::string user;
    std::string authkey;
};

// Log 配置
struct ThridLogConfig {
    std::string logPath  = "logs/";
    std::string logLevel = "info";
};

// XML 配置
struct ThridXmlConfig {
    std::string configPath = "config/server.xml";
};

// 平台管理器配置
struct ThridPlatformMgrConfig {
    std::string platform      = "dev";
    std::string traceEndpoint;   // 链路追踪上报地址
    bool        enableTrace   = false;
};

// =============================================================================
// ThridServiceConfig — 统一配置容器
// 每个 ThridService 实例只用对应自身类型的配置字段
// =============================================================================
struct ThridServiceConfig {
    ThridMysqlConfig        mysql;
    ThridRedisConfig        redis;
    ThridLogConfig          log;
    ThridXmlConfig          xml;
    ThridPlatformMgrConfig  platform;
};
