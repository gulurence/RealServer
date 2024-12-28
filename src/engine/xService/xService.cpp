
#include "xService.h"

#include "xBase/xXMLParser.h"


#include <iostream>
#include <boost/asio.hpp>
#include <vector>

xService::xService(const ServiceID& u64ServiceID, const char* pszServiceName) {
    m_u64ServiceID = u64ServiceID;
    //
    bzero(m_pszServiceName, sizeof(m_pszServiceName));
    strncpy(m_pszServiceName, pszServiceName, MAX_NAMESIZE - 1);
    //
    //SetServiceState(XSERVICE_CREATE);
    //
    XINF("Create [%s]", pszServiceName);



    m_ptrServiceScheduler = std::make_shared<ServiceScheduler>(u64ServiceID);
}

xService::~xService() {

}

void xService::Release() {

    xXMLParser::clearSystem();
}

