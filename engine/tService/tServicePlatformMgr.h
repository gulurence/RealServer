#pragma once

#include "tThridService.h"

// tServicePlatformMgr — 平台管理 / 链路追踪 / 指标上报服务
// 用于：遥测数据上报、分布式链路追踪、运行时平台标识管理
class tServicePlatformMgr : public ThridService
{
public:
    tServicePlatformMgr(int32 id, const std::string& name)
        : ThridService(EST_PlatformMgr, id, name) {}

    virtual bool Init(const ThridServiceConfig& cfg) override;
    virtual void Release() override;

    // --- 平台接口 ---
    const std::string& GetPlatform() const { return m_strPlatform; }

    // 链路追踪事件
    void TraceEvent(const char* eventName, const char* data);

    // 运行时指标上报
    void ReportMetric(const char* metricName, int64 value);

private:
    std::string m_strPlatform     = "dev";
    std::string m_strTraceEndpoint;
    bool        m_bEnableTrace    = false;
};
