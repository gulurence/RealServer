#include "tThridService.h"
#include "xBase/xTime.h"
#include "xLog/xLog.h"

ThridService::ThridService(EThridServiceType enType, int32 id, const std::string& name)
    : m_enType(enType)
    , m_i32ID(id)
    , m_strName(name)
    , m_i64CreateTime(static_cast<int64>(xTime::getCurSec()))
{
    XLOG("[ThridService] Created: type=%d name=%s", (int)enType, name.c_str());
}

ThridService::~ThridService() {
    XLOG("[ThridService] Destroyed: type=%d name=%s", (int)m_enType, m_strName.c_str());
}
