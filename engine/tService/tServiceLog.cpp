#include "tServiceLog.h"
#include "xLog/xLog.h"

bool tServiceLog::Init(const ThridServiceConfig& cfg) {
    if (m_bInitialized) {
        XWRN("[tServiceLog] Already initialized: %s", m_strName.c_str());
        return true;
    }

    // xLog 已在 main.cpp 启动早期通过 xLog::Init() 初始化。
    // 这里仅做服务注册标记。
    const auto& lc = cfg.log;
    XLOG("[tServiceLog] Init ok: %s (path=%s, level=%s)",
         m_strName.c_str(), lc.logPath.c_str(), lc.logLevel.c_str());

    m_bInitialized = true;
    return true;
}

void tServiceLog::Release() {
    if (!m_bInitialized) return;

    XLOG("[tServiceLog] Released: %s", m_strName.c_str());
    m_bInitialized = false;
}
