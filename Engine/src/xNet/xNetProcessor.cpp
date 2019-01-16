#include "xNetProcessor.h"


xNetProcessor::xNetProcessor(int fd, const char *n)
:m_stSock(fd)
{
    setNPState(NP_CREATE);

    m_u64Id = 0;
    setName(n);
    m_n32Epfd = 0;

    bzero(&m_stIp, sizeof(m_stIp));
    m_u16Port = 0;
}

xNetProcessor::~xNetProcessor()
{
}

void xNetProcessor::setName(const char *n)
{
    bzero(m_arrcName, sizeof(m_arrcName));
    if (n)
    {
        strncpy(m_arrcName, n, MAX_NAMESIZE-1);
    }
}

void xNetProcessor::disconnect()
{
    m_stSock.close();
    XLOG("[Socket]%s disconnect %p", m_arrcName, this);
    setNPState(NP_CLOSE);
}

void xNetProcessor::addEpoll(int ep)
{
#ifndef _WINDOWS
    m_n32Epfd = ep;
    epoll_event ev;
    bzero(&ev, sizeof(ev));
    ev.data.fd = m_stSock.get_fd();
    ev.data.ptr = this;
    ev.events = EPOLLIN|EPOLLOUT|EPOLLET;
    epoll_ctl(ep, EPOLL_CTL_ADD, m_stSock.get_fd(), &ev);
    setNPState(NP_VERIFIED);
#ifdef _LX_DEBUG
    XDBG("[添加Epoll],%s,epfd:%d, fd:%d", name, m_n32Epfd, m_stSock.get_fd());
#endif
#else

#endif
}

void xNetProcessor::delEpoll()
{
#ifndef _WINDOWS
    epoll_event ev;
    bzero(&ev, sizeof(ev));
    ev.data.fd = m_stSock.get_fd();
    ev.data.ptr = this;
    ev.events = EPOLLIN|EPOLLOUT|EPOLLET;
    epoll_ctl(m_n32Epfd, EPOLL_CTL_DEL, m_stSock.get_fd(), &ev);
#ifdef _LX_DEBUG
    XDBG("[删除Epoll],%s,epfd:%d, fd:%d", name, m_n32Epfd, m_stSock.get_fd());
#endif
#else

#endif
}

bool xNetProcessor::readCmd()
{
    return m_stSock.readToBuf();
}

bool xNetProcessor::sendCmd()
{
    return m_stSock.sendCmd();
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

    if (!m_stSock.sendCmd(cmd, len))
    {
        XERR("sendCmd failed %p", this);
        //post_disconnect();
        return false;
    }
    /*
    else
    {
        struct epoll_event ev;
        ev.data.fd = m_stSock.get_fd();
        ev.events = EPOLLIN|EPOLLOUT|EPOLLET;
        ev.data.ptr = this;
        epoll_ctl(m_n32Epfd, EPOLL_CTL_MOD, m_stSock.get_fd(), &ev);
    }
    */
    return true;
}

bool xNetProcessor::getCmd(unsigned char *&cmd, unsigned short &len)
{
    return m_stSock.getCmd(cmd, len);
}

bool xNetProcessor::popCmd()
{
    return m_stSock.popCmd();
}
