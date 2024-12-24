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

    GateService *pService = new GateService("GateService",26532);
    pService->thread_start();

    getchar();
    getchar();
    getchar();
    getchar();

    return 0;
}



