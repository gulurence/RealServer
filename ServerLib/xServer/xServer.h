#ifndef _X_SERVER_H_
#define _X_SERVER_H_
#include "xDefine.h"
#include "xSocket.h"
#include "xThread.h"
#include "xTime.h"
#include <map>
#ifndef _WINDOWS
#include "SystemCmd.h"
#include <netinet/in.h>
#include <sys/epoll.h>
#endif

class xServer;
class xTask;
class xClient;
class xNetProcessor;

class ListenThread : public xThread
{
public:
    ListenThread(const char *n, int p);
    virtual ~ListenThread();

    virtual bool accept(int sockfd, sockaddr_in &addr)=0;
    bool thread_init();
    void thread_proc();
    void thread_stop();

private:
    bool thread_proc_can_accept_windows();
    bool thread_proc_can_accept_linux();

public:

    // 这里是管理监听句柄的 windows 用 select linux 用 epoll
#ifndef _WINDOWS
    struct epoll_event acceptEvent;
#else

#endif

    int listenSock;
    int listenPort;
    int listen_epfd;

    char name[MAX_NAMESIZE];
};
//抽象服务器
class xServer
{
    friend class ListenThread;
    public:
        xServer(const char *n);
        virtual ~xServer();

        enum ServerState
        {
            SERVER_CREATE = 0,
            SERVER_INIT,        //connect other server
            SERVER_RUN,            //all server connected, do work
            SERVER_SAVE,        //保存数据，仅场景进程用
            SERVER_STOP,        //will server_stop
            SERVER_FINISH,        //all done
        };

        void run();
        void server_stop()
        {
            setServerState(SERVER_STOP);
        }
        bool closeConn(xNetProcessor *np);
        virtual void v_closeConn(xNetProcessor *np){}
        char serverName[MAX_NAMESIZE];//server name
        ServerState getServerState(){return server_state;}

    protected:
        bool serverCallback();
        void final();

        virtual bool v_serverCallback(){return true;}
        virtual void v_final()=0;
        virtual bool v_stop() {return true;}

        void setServerState(ServerState s);

        virtual bool processNetwork()=0;

        ServerState server_state;

        pthread_rwlock_t _close_critical;
        pthread_rwlock_t _serversave_critical;
        std::map<xNetProcessor *, time_t> _close_list;//np:time
        typedef std::map<xNetProcessor *, time_t>::iterator _close_iter;
};
#endif

