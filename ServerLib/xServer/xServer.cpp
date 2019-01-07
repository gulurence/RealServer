#include <string>
#include "xServer.h"
#include "xTask.h"
#include "xClient.h"
#include <errno.h>
#include "xXMLParser.h"
#include <signal.h>

#ifndef _WINDOWS
#include <arpa/inet.h>
#endif



ListenThread::ListenThread(const char *n, int p)
:listenPort(p)
{
    bzero(name, sizeof(name));
    strncpy(name, n, MAX_NAMESIZE);

    listenSock = -1;
    listen_epfd = -1;
}

ListenThread::~ListenThread()
{
    if (-1!=listenSock)
        SAFE_CLOSE_SOCKET(listenSock);
    if (-1!=listen_epfd)
        SAFE_CLOSE_SOCKET(listen_epfd);
}

void ListenThread::thread_stop()
{
    xThread::thread_stop();
}

bool ListenThread::thread_init()
{
    XLOG("[%s]init...", name);

    if ((listenSock = socket(AF_INET, SOCK_STREAM, 0)) <0)
    {
        XERR("[%s]socket() failed %s", name, strerror(errno));
        listenSock = -1;
        return false;
    }
    XLOG("[Socket]open %d", listenSock);

    int re = 1;
#ifndef _WINDOWS
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &re, sizeof(re));
    listen_epfd = epoll_create(256);
    if (listen_epfd<0)
    {
        XERR("[%s]epoll_create() failed %s", name, strerror(errno));
        listen_epfd = -1;
        return false;
    }
    XLOG("[Socket]open %d epfd", listen_epfd);
#else
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (char*)&re, sizeof(re));
#endif

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(listenPort);
    if (::bind(listenSock, (sockaddr *) &addr, sizeof(addr)) == -1)
    {
        XERR("[%s]bind() failed %s", name, strerror(errno));
        return false;
    }
    XLOG("[%s]bind %s:%d", name, inet_ntoa(addr.sin_addr), listenPort);

    if(::listen(listenSock, 256) == -1)
    {
        XERR("[%s]listen失败 %s", name, strerror(errno));
        return false;
    }

#ifndef _WINDOWS
    acceptEvent.data.fd = listenSock;
    acceptEvent.events = EPOLLIN|EPOLLERR;
    epoll_ctl(listen_epfd, EPOLL_CTL_ADD, listenSock, &acceptEvent);
#else
#endif

    return true;
}

void ListenThread::thread_proc()
{
    XLOG("[%s]start", name);

    int cfd = 0;
    //int nfds = 0;
    sockaddr_in caddr;
    int addrlen=sizeof(caddr);

    thread_setState(xThread::THREAD_RUN);
    XLOG("[%s]开始监听...", name);

    while(thread_getState()==xThread::THREAD_RUN)
    {
#ifdef _WINDOWS
        if(thread_proc_can_accept_windows())
#else
        if(thread_proc_can_accept_linux())
#endif
        {
            bzero(&caddr, sizeof(caddr));
#ifndef _WINDOWS
            cfd = ::accept(listenSock, (struct sockaddr*)&caddr, (socklen_t *)&addrlen);
#else
            cfd = ::accept(listenSock, (struct sockaddr*)&caddr, (int *)&addrlen);
#endif
            if(-1 == cfd)
            {
                XERR("accept 出错,errno:%u,%s.",errno, strerror(errno));
                continue;
            }

            if (thread_getState()==xThread::THREAD_RUN)
                this->accept(cfd, caddr);
            else
                SAFE_CLOSE_SOCKET(cfd);
        }
//        UInt64 _e = frameTimer.uElapse();
//        if (_e < 50000)
//            usleep(50000-_e);
    }

    XLOG("[%s]停止", name);
}

bool ListenThread::thread_proc_can_accept_windows()
{


    return true;
}


bool ListenThread::thread_proc_can_accept_linux()
{
#ifndef _WINDOWS
    //        xTime frameTimer;
    int nfds = epoll_wait(listen_epfd, &acceptEvent, 1, 50);
    if (1==nfds)
    {
        if (acceptEvent.events & EPOLLIN)
        {
            return true;
        }
        else
        {
            XERR("监听出错 关闭.,errno:%u,%s.",errno, strerror(errno));
        }
    }
#endif
    return false;
}

void handle_pipe(int s)
{
    XLOG("[handle_pipe]");
}

xServer::xServer(const char *n)
{
    printf("\nstart ZoneServer:%s \n",n);
    bzero(serverName, sizeof(serverName));
    strncpy(serverName, n, MAX_NAMESIZE);
    
    int ret = pthread_rwlock_init(&_close_critical, 0);
    if (0!=ret)
        XERR("初始化读写锁失败 ret=%d", ret);
    ret = pthread_rwlock_init(&_serversave_critical, 0);
    if (0!=ret)
        XERR("初始化存储锁失败 ret=%d", ret);

    setServerState(SERVER_CREATE);

    srvLog = new xLog(serverName);
    if (!srvLog || !srvLog->init(serverName))
        fprintf(stderr, "[%s]srvLog init failed", serverName);

#ifndef _WINDOWS
    struct sigaction action;
    action.sa_handler = handle_pipe;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGPIPE, &action, NULL);
#endif
}

xServer::~xServer()
{
    pthread_rwlock_destroy(&_close_critical);
    pthread_rwlock_destroy(&_serversave_critical);

    XLOG_END;
}

void xServer::final()
{
    XLOG("[%s]final", serverName);

    v_final();

    _close_iter tmp_close;
    xNetProcessor *clo = 0;
    pthread_rwlock_wrlock(&_close_critical);
    for (_close_iter it=_close_list.begin(); it!=_close_list.end(); )
    {
        tmp_close = it++;

        clo = tmp_close->first;

        XLOG("[%s]%s delete %p", serverName, clo->m_arrcName, clo);
        SAFE_DELETE(clo);
    }
    _close_list.clear();
    pthread_rwlock_unlock(&_close_critical);

    xXMLParser::clearSystem();
}

void xServer::setServerState(ServerState s)
{
    pthread_rwlock_wrlock(&_serversave_critical);
    server_state = s;
    pthread_rwlock_unlock(&_serversave_critical);

    switch (server_state)
    {
        case SERVER_CREATE:    
            {
                XLOG("[%s],创建成功 ", serverName);
            }
            break;
        case SERVER_INIT:    //connect other server
            {
                XLOG("[%s],准备初始化数据 ", serverName);
            }
            break;
        case SERVER_RUN://all server connected, do work
            {
                XLOG("[%s],初始化完毕，开始运行 ", serverName);
            }
            break;
        case SERVER_SAVE:
            {
                XLOG("[%s],保存数据，即将终止 ", serverName);
            }
            break;
        case SERVER_STOP://will server_stop
            {
                XLOG("[%s],主循环结束，即将终止 ", serverName);
            }
            break;
        case SERVER_FINISH://all done
            {
                XLOG("[%s],进程终止 ", serverName);
            }
            break;
        default:
            break;
    }
}

//主线程和子线程公用
bool xServer::closeConn(xNetProcessor *np)
{
    if (!np) return false;

    bool ret = false;

    pthread_rwlock_wrlock(&_close_critical);
    if (_close_list.find(np)!=_close_list.end())
    {
        XERR("[%s]re close %s %p", serverName, np->m_arrcName, np);
    }
    else
    {
        v_closeConn(np);
        _close_list[np] = time(0);
        XDBG("[%s]close %s %p", serverName, np->m_arrcName, np);
        ret = true;
    }
    pthread_rwlock_unlock(&_close_critical);

    return ret;
}

bool xServer::serverCallback()
{
//    xTime frameTimer;

    if (!v_serverCallback()) 
    {
        XERR("[%s]v_serverCallback err", serverName);
        return false;
    }

    if (!processNetwork()) 
    {
        XERR("[%s]v_serverCallback err", serverName);
        return false;
    }

    _close_iter tmp_close;
    xNetProcessor *clo = 0;
    pthread_rwlock_wrlock(&_close_critical);
    for (_close_iter it=_close_list.begin(); it!=_close_list.end(); )
    {
        tmp_close = it++;

        if ((time(0)-tmp_close->second)>=5)
        {
            clo = tmp_close->first;
            clo->delEpoll();
            clo->disconnect();


            XLOG("[%s],_close_list 删除连接,%s(%llu),%p", serverName, clo->m_arrcName, clo->m_u64Id, clo);
            _close_list.erase(tmp_close);
            SAFE_DELETE(clo);
        }
    }
    pthread_rwlock_unlock(&_close_critical);

//    UInt64 _e = frameTimer.uElapse();
//    if (_e < 50000)
//        usleep(50000-_e);
//    if (_e > 100000)
//        XDBG("[%s],帧耗时 %u 微秒", serverName, _e);

    return true;
}

void xServer::run()
{
#define FINAL_RETURN {final();return;}
    //daemon(1,1);
    setServerState(SERVER_INIT);
    xXMLParser::initSystem();

    Tools::initGlobal();

    srand(xTime::getCurUSec());

    while (serverCallback());
    setServerState(SERVER_STOP);

    final();
    setServerState(SERVER_FINISH);
    //XLOG_END;
#undef FINAL_RETURN
}
