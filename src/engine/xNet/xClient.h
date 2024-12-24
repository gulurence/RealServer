#pragma once

#include "../xNet/xSocket.h"
#include "../xNet/xNetProcessor.h"

class xClient : public xNetProcessor
{
public:
    xClient(int fd);
    virtual ~xClient();

    bool connect(const char *ip, int port);

    bool isClient(){return true;}
    bool isTask(){return false;}
};



