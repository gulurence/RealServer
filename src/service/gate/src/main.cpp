#include <iostream>
#include <coroutine>
#include <queue>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>
#include <string>
#include <functional>

#include "xLog/xLog.h"
#include "rpc/myservice.pb.h"
#include "msg/login.pb.h"

#include "xEvent/xEventDispatcher.h"
#include "xService/xServiceMgr.h"

int main() {
    xLog::get_mutable_instance().Init();
    ServiceLogin* pServiceLogin = new ServiceLogin(100, "service_100");
    bool bRegistRet = xServiceMgr::getMe().Regist(pServiceLogin);
    if (!bRegistRet) {
        return 0;
    }

    int index = 1;
    while (true) {
        if (index < 1000) {
            PbMsg::LoginRequest* reqMsg = new PbMsg::LoginRequest();
            PbMsg::LoginResponse* rspMsg = new PbMsg::LoginResponse();

            reqMsg->set_index(index);
            rspMsg->set_index(index);
            index++;

            PBEventPtr ptrEvent = std::make_shared<xPBEvent>(reqMsg, rspMsg);
            // 
            xEventDispatcher::getMe().OnMsg(1, 100, 100, 100, ptrEvent);
            xEventDispatcher::getMe().OnMsg(1, 100, 100, 101, ptrEvent);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return 0;
}










//
// //echo_server_with_as_tuple_default.cpp
// //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// //Copyright (c) 2003-2024 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// //Distributed under the Boost Software License, Version 1.0. (See accompanying
// //file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//


/*
#include "xLog/xLog.h"
#include "GateService.h"

#include "xNet/xNet.h"



xNet* gNet = nullptr;
uint64 gCID = 01;
uint64 gMsgIndex = 0;

uint64 NetAccept(tcp_socket& socket) {
    return gCID++;
}

void NetMsg(tcp_socket& socket, uint64 u64CID, NetPackageSharedPtr pMsg) {
    NetPackageHeadSharedPtr pPackageHead = (NetPackageHeadSharedPtr)pMsg->pPackageData;
    XERR("Recive Package ");
    XERR("Recive Package size:%d,cid:%ld,gMsgIndex:%d", pPackageHead->u16BodySize, u64CID, gMsgIndex++);
    //boost::asio::async_write(socket, boost::asio::buffer((char*)pMsg, pMsg->u16BodySize + NetPackageHeadSize));
}

int main() {

    //log4cxx::PropertyConfigurator::configureAndWatch("log4cxx.properties");
    //log4cxx::LoggerPtr rootLogger = log4cxx::Logger::getRootLogger();
    //rootLogger->info("打发士大夫");

    //log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("root");
    // 使用 logger 记录日志
    //LOG4CXX_INFO(logger, "This is an info message");

    xLog::get_mutable_instance().Init();
    //GateService *pService = new GateService("GateService",26532);
    //pService->thread_start();

    gNet = new xNet("TestService", 26532, NetAccept, NetMsg, 1024);
    gNet->thread_start();

    getchar();
    getchar();
    getchar();
    getchar();

    return 0;
}
*/




