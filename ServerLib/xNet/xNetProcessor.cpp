#include "xNetProcessor.h"


xNetProcessor::xNetProcessor(int fd, const char *n)
:sock(fd)
{
    setNPState(NP_CREATE);

    id = 0;
    setName(n);
    epfd = 0;

    bzero(&ip, sizeof(ip));
    port = 0;
}

xNetProcessor::~xNetProcessor()
{
}

void xNetProcessor::setName(const char *n)
{
    bzero(name, sizeof(name));
    if (n)
    {
        strncpy(name, n, MAX_NAMESIZE);
    }
}

void xNetProcessor::disconnect()
{
    sock.close();
    XLOG("[Socket]%s disconnect %p", name, this);
    setNPState(NP_CLOSE);
}

void xNetProcessor::addEpoll(int ep)
{
#ifndef _WINDOWS
    epfd = ep;
    epoll_event ev;
    bzero(&ev, sizeof(ev));
    ev.data.fd = sock.get_fd();
    ev.data.ptr = this;
    ev.events = EPOLLIN|EPOLLOUT|EPOLLET;
    epoll_ctl(ep, EPOLL_CTL_ADD, sock.get_fd(), &ev);
    setNPState(NP_VERIFIED);
#ifdef _LX_DEBUG
    XDBG("[添加Epoll],%s,epfd:%d, fd:%d", name, epfd, sock.get_fd());
#endif
#else

#endif
}

void xNetProcessor::delEpoll()
{
#ifndef _WINDOWS
    epoll_event ev;
    bzero(&ev, sizeof(ev));
    ev.data.fd = sock.get_fd();
    ev.data.ptr = this;
    ev.events = EPOLLIN|EPOLLOUT|EPOLLET;
    epoll_ctl(epfd, EPOLL_CTL_DEL, sock.get_fd(), &ev);
#ifdef _LX_DEBUG
    XDBG("[删除Epoll],%s,epfd:%d, fd:%d", name, epfd, sock.get_fd());
#endif
#else

#endif
}

bool xNetProcessor::readCmd()
{
    return sock.readToBuf();
}

bool xNetProcessor::sendCmd()
{
    return sock.sendCmd();
}

bool xNetProcessor::sendCmd(const void *cmd, unsigned short len)
{
    if (getNPState()==NP_CLOSE ||  NP_DISCONNECT==getNPState()) return false;
    if (!cmd || !len) return false;
#ifndef _WUWENJUAN_DEBUG
#ifndef _LX_DEBUG
#ifndef _LHY_DEBUG
    //XDBG("sendCmd %p state:%u %p %u", this,getState(), cmd, len);
#endif
#endif
#endif

    if (!sock.sendCmd(cmd, len))
    {
        XERR("sendCmd failed %p", this);
        //post_disconnect();
        return false;
    }
    /*
    else
    {
        struct epoll_event ev;
        ev.data.fd = sock.get_fd();
        ev.events = EPOLLIN|EPOLLOUT|EPOLLET;
        ev.data.ptr = this;
        epoll_ctl(epfd, EPOLL_CTL_MOD, sock.get_fd(), &ev);
    }
    */
    return true;
}

bool xNetProcessor::getCmd(unsigned char *&cmd, unsigned short &len)
{
    return sock.getCmd(cmd, len);
}

bool xNetProcessor::popCmd()
{
    return sock.popCmd();
}
