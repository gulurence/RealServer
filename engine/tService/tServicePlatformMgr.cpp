/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: tServicePlatformMgr.cpp
      Purpose:   平台管理服务实现

      tServicePlatformMgr::Init 从 ThridPlatformMgrConfig 读取 platform、traceEndpoint、enableTrace 并登记追踪开关
      TraceEvent 与 ReportMetric 通过 xLog 输出链路追踪事件与运行时指标
  *****************************************************************/

#include "tServicePlatformMgr.h"
#include "xLog/xLog.h"

bool tServicePlatformMgr::Init(const ThridServiceConfig &cfg)
{
    if (m_bInitialized)
    {
        XWRN("[tServicePlatformMgr] Already initialized: %s", m_strName.c_str());
        return true;
    }

    const auto &pc = cfg.platform;
    m_strPlatform = pc.platform;
    m_strTraceEndpoint = pc.traceEndpoint;
    m_bEnableTrace = pc.enableTrace;

    if (m_bEnableTrace && !m_strTraceEndpoint.empty())
    {
        // TODO: 连接追踪端点 (OpenTelemetry / custom tracing)
        XLOG("[tServicePlatformMgr] Trace enabled: endpoint=%s", m_strTraceEndpoint.c_str());
    }

    m_bInitialized = true;
    XLOG("[tServicePlatformMgr] Init ok: %s (platform=%s, trace=%s)", m_strName.c_str(),
         m_strPlatform.c_str(), m_bEnableTrace ? "on" : "off");
    return true;
}

void tServicePlatformMgr::Release()
{
    if (!m_bInitialized)
    {
        return;
    }

    // Flush pending traces/metrics before shutdown
    if (m_bEnableTrace)
    {
        XLOG("[tServicePlatformMgr] Flushing traces...");
    }

    XLOG("[tServicePlatformMgr] Released: %s", m_strName.c_str());
    m_bInitialized = false;
}

void tServicePlatformMgr::TraceEvent(const char *eventName, const char *data)
{
    if (!m_bEnableTrace)
    {
        return;
    }
    XLOG("[PlatformTrace] %s: %s", eventName, data ? data : "");
}

void tServicePlatformMgr::ReportMetric(const char *metricName, int64 value)
{
    XLOG("[PlatformMetric] %s = %lld", metricName, value);
}
