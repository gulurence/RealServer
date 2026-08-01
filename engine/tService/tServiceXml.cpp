#include "tServiceXml.h"
#include "xConfig/xServerConfig.h"
#include "xLog/xLog.h"

bool tServiceXml::Init(const ThridServiceConfig& cfg) {
    if (m_bInitialized) {
        XWRN("[tServiceXml] Already initialized: %s", m_strName.c_str());
        return true;
    }

    // CServerConfig 由 main.cpp 在早期加载。
    // 如果尚未加载，则在此处按配置路径加载。
    const auto& xc = cfg.xml;
    if (!xc.configPath.empty()) {
        if (!CServerConfig::getMe().Load(xc.configPath)) {
            XERR("[tServiceXml] Init failed: cannot load %s", xc.configPath.c_str());
            return false;
        }
    }

    m_bInitialized = true;
    XLOG("[tServiceXml] Init ok: %s (config=%s)",
         m_strName.c_str(), xc.configPath.c_str());
    return true;
}

void tServiceXml::Release() {
    if (!m_bInitialized) return;

    XLOG("[tServiceXml] Released: %s", m_strName.c_str());
    m_bInitialized = false;
}
