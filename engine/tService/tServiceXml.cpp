/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: tServiceXml.cpp
      Purpose:   XML 配置服务封装实现

      tServiceXml::Init 在 CServerConfig 未加载时按 ThridXmlConfig 路径 Load 配置
      已加载时跳过重复加载以避免覆盖命令行参数（--type/--name/--port/--platform）
  *****************************************************************/

#include "tServiceXml.h"
#include "xConfig/xServerConfig.h"
#include "xLog/xLog.h"

bool tServiceXml::Init(const ThridServiceConfig &cfg)
{
    if (m_bInitialized)
    {
        XWRN("[tServiceXml] Already initialized: %s", m_strName.c_str());
        return true;
    }

    // CServerConfig 由 main.cpp 在早期加载。
    // 如果尚未加载，则在此处按配置路径加载。
    // 注意: 已加载时绝不能重复 Load, 否则会覆盖 main.cpp 解析的命令行参数 (--type/--name/--port/--platform)。
    const auto &xc = cfg.xml;
    if (!xc.configPath.empty() && !CServerConfig::getMe().IsLoaded())
    {
        if (!CServerConfig::getMe().Load(xc.configPath))
        {
            XERR("[tServiceXml] Init failed: cannot load %s", xc.configPath.c_str());
            return false;
        }
    }

    m_bInitialized = true;
    XLOG("[tServiceXml] Init ok: %s (config=%s)", m_strName.c_str(), xc.configPath.c_str());
    return true;
}

void tServiceXml::Release()
{
    if (!m_bInitialized)
    {
        return;
    }

    XLOG("[tServiceXml] Released: %s", m_strName.c_str());
    m_bInitialized = false;
}
