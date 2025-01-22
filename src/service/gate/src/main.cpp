

#include <grpcpp/grpcpp.h>
#include "rpc/myservice.grpc.pb.h"
#include "msg/login.pb.h"
#include "rpc/myservice.pb.h"

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

#include "xEvent/xEventDispatcher.h"
#include "xService/xServiceMgr.h"

#include "xRpc/xRpc.h"

int main() {
    // 初始化日志系统
    xLog::get_mutable_instance().Init();

    //  链接到grpc服务器
    CRpcService::getMe().ConnectToLocalServer("test_rpc", 50051);

    {
        // 定义注册 service 100
        ServiceLogin* pServiceLogin = new ServiceLogin(100, "service_100");
        bool bRegistRet = xServiceMgr::getMe().Regist(pServiceLogin);
        if (!bRegistRet) {
            return 0;
        }
    }

    {
        // 定义注册 service 101
        ServiceLogin* pServiceLogin = new ServiceLogin(101, "service_101");
        bool bRegistRet = xServiceMgr::getMe().Regist(pServiceLogin);
        if (!bRegistRet) {
            return 0;
        }
    }

    {
        // 定义注册 service 102
        ServiceLogin* pServiceLogin = new ServiceLogin(102, "service_102");
        bool bRegistRet = xServiceMgr::getMe().Regist(pServiceLogin);
        if (!bRegistRet) {
            return 0;
        }
    }

    int index1 = 1000000;
    int index2 = 2000000;
    int index3 = 3000000;
    int index4 = 4000000;
    int index5 = 5000000;
    while (true) {
        // 发送1000次消息
        if (index1 < 1001000) {
            {
                PbMsg::LoginRequest* reqMsg = new PbMsg::LoginRequest();
                PbMsg::LoginResponse* rspMsg = new PbMsg::LoginResponse();

                reqMsg->set_index(index1);
                index1++;

                PBEventPtr ptrEvent = std::make_shared<xPBEvent>(reqMsg, rspMsg);
                // 向service 100 发送 需要协程处理的 100号 消息
                xEventDispatcher::getMe().OnMsg(1, 100, 100, 100, ptrEvent);
            }

            {
                PbMsg::LoginRequest* reqMsg = new PbMsg::LoginRequest();
                PbMsg::LoginResponse* rspMsg = new PbMsg::LoginResponse();

                reqMsg->set_index(index2);
                index2++;
                PBEventPtr ptrEvent = std::make_shared<xPBEvent>(reqMsg, rspMsg);
                // 向service 101 发送 需要协程处理的 101号 消息
                xEventDispatcher::getMe().OnMsg(1, 101, 100, 101, ptrEvent);
            }

            {
                PbMsg::LoginRequest* reqMsg = new PbMsg::LoginRequest();
                PbMsg::LoginResponse* rspMsg = new PbMsg::LoginResponse();

                reqMsg->set_index(index3);
                index3++;
                PBEventPtr ptrEvent = std::make_shared<xPBEvent>(reqMsg, rspMsg);
                // 向service 100 发送 不需要协程处理的 102号 消息
                xEventDispatcher::getMe().OnMsg(1, 100, 100, 102, ptrEvent);
            }

            {
                PbMsg::LoginRequest* reqMsg = new PbMsg::LoginRequest();
                PbMsg::LoginResponse* rspMsg = new PbMsg::LoginResponse();

                reqMsg->set_index(index4);
                index4++;
                PBEventPtr ptrEvent = std::make_shared<xPBEvent>(reqMsg, rspMsg);
                // 向service 101 发送 不需要协程处理的 102号 消息
                xEventDispatcher::getMe().OnMsg(1, 101, 100, 102, ptrEvent);
            }

            {
                PbMsg::LoginRequest* reqMsg = new PbMsg::LoginRequest();
                PbMsg::LoginResponse* rspMsg = new PbMsg::LoginResponse();

                reqMsg->set_index(index5);
                index5++;
                PBEventPtr ptrEvent = std::make_shared<xPBEvent>(reqMsg, rspMsg);
                // 向service 102 发送 不需要协程处理的 102号 消息
                xEventDispatcher::getMe().OnMsg(1, 102, 100, 102, ptrEvent);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return 0;
}







//#include <iostream>
//#include <memory>
//#include <string>
//#include <grpcpp/grpcpp.h>
//#include "rpc/myservice.grpc.pb.h"
//
//using grpc::Server;
//using grpc::ServerBuilder;
//using grpc::ServerContext;
//using grpc::Status;
//using myservice::MyService;
//using myservice::HelloRequest;
//using myservice::HelloReply;
//
//int index = 0;
//// 实现 Greeter 服务
//class GreeterServiceImpl final : public MyService::Service
//{
//public:
//    Status SayHello(ServerContext* context, const HelloRequest* request, HelloReply* reply) override {
//        printf("%d %s\n", index++,request->name().c_str());
//        std::string prefix = "Hello ";
//        reply->set_message(prefix + request->name());
//        std::this_thread::sleep_for(std::chrono::milliseconds(20));
//        return Status::OK;
//    }
//};
//
//void RunServer() {
//    std::string server_address("0.0.0.0:50051");
//    GreeterServiceImpl service;
//
//    ServerBuilder builder;
//    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
//    builder.RegisterService(&service);
//
//    std::unique_ptr<Server> server(builder.BuildAndStart());
//    std::cout << "Server listening on " << server_address << std::endl;
//
//    server->Wait();
//}
//
//int main() {
//    RunServer();
//    return 0;
//}
//




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




