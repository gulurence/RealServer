//
// //echo_server_with_as_tuple_default.cpp
// //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// //Copyright (c) 2003-2024 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// //Distributed under the Boost Software License, Version 1.0. (See accompanying
// //file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "xLog/xLog.h"
#include "GateService.h"

 

int main() {

    //log4cxx::PropertyConfigurator::configureAndWatch("log4cxx.properties");
    //log4cxx::LoggerPtr rootLogger = log4cxx::Logger::getRootLogger();
    //rootLogger->info("打发士大夫");

    //log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("root");
    // 使用 logger 记录日志
    //LOG4CXX_INFO(logger, "This is an info message");

    xLog::get_mutable_instance().Init();
    GateService *pService = new GateService("GateService",26532);
    pService->thread_start();

    getchar();
    getchar();
    getchar();
    getchar();

    return 0;
}



