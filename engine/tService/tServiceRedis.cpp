#include "tServiceRedis.h"
#include "xRedis/RedisMgr.h"
#include "xLog/xLog.h"

bool tServiceRedis::Init(const ThridServiceConfig& cfg) {
    if (m_bInitialized) {
        XWRN("[tServiceRedis] Already initialized: %s", m_strName.c_str());
        return true;
    }

    auto redisCfg = cfg.redis;
    redisCfg.title = m_strName;

    if (!CRedisPoolMgr::getMe().ConnectToRedis(redisCfg)) {
        XERR("[tServiceRedis] Init failed: %s @ %s:%d",
             m_strName.c_str(), redisCfg.host.c_str(), redisCfg.port);
        return false;
    }

    m_bInitialized = true;
    XLOG("[tServiceRedis] Init ok: %s @ %s:%d (max_conn=%d)",
         m_strName.c_str(), redisCfg.host.c_str(), redisCfg.port, redisCfg.max_conn);
    return true;
}

void tServiceRedis::Release() {
    if (!m_bInitialized) return;

    // CRedisPoolMgr 连接池由析构清理
    XLOG("[tServiceRedis] Released: %s", m_strName.c_str());
    m_bInitialized = false;
}
