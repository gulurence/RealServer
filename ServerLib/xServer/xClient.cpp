#include "xClient.h"
#ifndef _WINDOWS
#include <netinet/in.h>
#include<arpa/inet.h>
#endif



xClient::xClient(int fd)
:xNetProcessor(fd)
{
}

xClient::~xClient()
{
}

bool xClient::connect(const char *ip, int port)
{
    struct sockaddr_in server;
    bzero(&server,sizeof(server));
    server.sin_family = AF_INET; 
    server.sin_addr.s_addr = inet_addr(ip); 
    server.sin_port = htons(port);

    int ret = ::connect(sock.get_fd(), (sockaddr *)&server, sizeof(sockaddr_in));
    if (0!=ret)
    {
        //XERR("connect() failed with error %d %p", ret, this);
        return false;
    }

/*
    fd_set r; 
    FD_ZERO(&r); 
    FD_SET(sock.get_fd(), &r);

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec =0; 

    ret = select(0,0,&r,0,&timeout);
    if (ret <= 0)
    {
        XERR("select() failed with error %d %p", ret, this);
        return false;
    }
    */

    sock.setNonBlock();
    return true;
}

