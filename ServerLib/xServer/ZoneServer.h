#pragma once
#include "xServer.h"
#include "xDBConnPool.h"
#include "xNetProcessor.h"

#define SERVER_HEARTBEAT 20
//游戏分区服务
class ZoneServer;
class ZoneListenThread : public ListenThread
{
    public:
        ZoneListenThread(const char *n, int p, ZoneServer *ps);
        ~ZoneListenThread(){}

        bool accept(int sockfd, sockaddr_in &addr);

        ZoneServer *pServer;
};

class ServerClient
{
    friend class ZoneServer;
    public:
        ServerClient()
        {
            np = NULL;
            _heartbeat = SERVER_HEARTBEAT;
        }
        ~ServerClient(){}

        bool sendCmd(const void *cmd, unsigned short len)
        {
            if (cmd && len && np)
                return np->sendCmd(cmd, len);
            return false;
        }

        bool getCmd(unsigned char *&cmd, unsigned short &len)
        {
            if (np)
                return np->getCmd(cmd, len);
            return false;
        }
        bool popCmd()
        {
            if (np)
                return np->popCmd();
            return false;
        }
        void put(unsigned char *cmd, unsigned short len)
        {
            if (np)
                np->put(cmd, len);
        }
        inline bool get(unsigned char *&cmd, unsigned short &len)
        {
            if (np)
                return np->get(cmd, len);
            return false;
        }
        void pop()
        {
            if (np)
                np->pop();
        }
        xNetProcessor *getNp()
        {
            return np;
        }
        const char* getName()
        {
            if (np) return np->name;
            return "";
        }
        bool check()
        {
            _heartbeat--;
            if (_heartbeat<=0)
                return false;
            return true;
        }
        void reset()
        {
            _heartbeat = SERVER_HEARTBEAT;
        }
        xNetProcessor *np;
    private:

        int _heartbeat;
};
//分区服务器
class ZoneServer : public xServer
{
    friend class ZoneListenThread;
    public:
        ZoneServer(const char *t, const char *n);
        ~ZoneServer();

        bool loadZoneInfo();
        bool loadServerList();

        bool checkConnectedServer(std::string t, std::string n="");
        void setConnectServerType(std::string t);
        ServerClient *getConnectedServer(std::string t, std::string n="");
        bool getConnectedServer(std::string t, std::string n, std::vector<ServerClient *> &list);

        virtual bool verifyServer(xNetProcessor *np, const char *t, const char *n);    //主线程

        bool sendCmdToServer(void *data, unsigned short len, const char *type, const char *name="");
        bool sendCmdToFirstServer(void *data, unsigned short len, const char *type);
        bool sendCmdToSession(void *data, unsigned short len);
        bool sendCmdToScene(void *data, unsigned short len);
        bool sendCmdToRecord(void *data, unsigned short len);
        bool sendCmdToLog(void *data, unsigned short len);

        char serverType[MAX_NAMESIZE];//server type
        bool r_init;

        char *GetServerIP();
        int getServerPort() const { return server_port;}

        UInt32 getZoneOpenTime()const{return zoneOpenTime;}

        UInt32 getPlatformID()const{return platformID;}
        const char *getPlatformName()const{return platformName;}
        UInt32 getZoneID()const{return zoneID;}
        const char *getZoneName()const{return zoneName;}

        bool getFullName(std::string &dest, std::string src, UInt32 zoneid);
        std::string getFullName(std::string src, UInt32 zoneid);
        bool getShortName(std::string &dest, std::string str);
        std::string getShortName(std::string src);

    private:
        UInt32 platformID;
        char platformName[MAX_NAMESIZE];
        UInt32 zoneID;
        char zoneName[MAX_NAMESIZE];

        typedef std::map<UInt32, std::string> ZoneMap;
        ZoneMap zoneMap;

    protected:
        virtual bool rInit(){return true;}
        virtual bool loadConfig(){return true;}
        virtual bool init(){return true;}
        virtual void v_final();

        bool startListen(int port);

        virtual bool accept(int sockfd, sockaddr_in &addr);

        virtual bool closeConn(xNetProcessor *np);

        virtual xTask *newTask(int sockfd);
        virtual xClient *newClient(int sockfd);

        bool v_serverCallback();

        virtual bool checkConnect()=0;//if all connected, server start ok

        virtual void sendVerify(xNetProcessor *, bool ret);
        virtual bool closeServer(xNetProcessor *);
        virtual void v_closeServer(xNetProcessor *)=0;

        bool connectServerByType(std::string t);

        bool processNetwork();
        bool processCmd(string serverName,xNetProcessor *np, unsigned char *buf, unsigned short len);
        virtual bool v_processCmd(string serverName,xNetProcessor *np, unsigned char *buf, unsigned short len)=0;

        virtual bool runAction();
        int server_port;

#ifndef _WINDOWS
        int server_epfd;//epfd
        struct epoll_event server_ev[MAX_SERVER_EVENT];
#else
        int        server_sefd;//epfd
        SOCKET            m_MinFD;
        SOCKET            m_MaxFD;
        INT                m_nFDSize ;
        //网络数据  
        enum{
            SELECT_BAK = 0,    //  当前系统中拥有的完整句柄数据   
            SELECT_USE = 1,    //  用于select调用的句柄数据  
            SELECT_MAX = 2, //  结构使用数量  
        }; 
        fd_set            m_ReadFDs[SELECT_MAX];
        fd_set            m_WriteFDs[SELECT_MAX];
        fd_set            m_ExceptFDs[SELECT_MAX];
        timeval            m_Timeout[SELECT_MAX];

        map<int,xNetProcessor *> clientMap;

        void            addSocket(xNetProcessor *t);
        void            removeSocket(xNetProcessor *t);
#endif

        ListenThread *listener;

        xTime checkConnectTimer;
        xTime heartBeatTimer;

        pthread_rwlock_t verifylist_critical;
        std::map<xNetProcessor *, time_t> verify_list;//task:time
        typedef std::map<xNetProcessor *, time_t>::iterator verify_iter;
        void setVerifyList(xNetProcessor *t, time_t);
        void removeVerifyList(xNetProcessor *task);
        bool inVerifyList(xNetProcessor *task);
        void verifyOk(xNetProcessor *task);
        xNetProcessor *getNpByIPAndPort(UInt32 ip, UInt32 port);
        virtual void v_verifyOk(xNetProcessor *task){}

        std::map<std::string, std::map<std::string, ServerClient *> > _serverList;//remote type:name:task
        typedef std::map<std::string, std::map<std::string, ServerClient *> >::iterator _server_type_iter;
        typedef std::map<std::string, ServerClient *>::iterator _server_iter;
        //MYSQL信息
        struct MysqlInfo
        {
            std::string server;//MYSQL地址
            std::string user;//用户
            std::string password;//密码
            std::string database;//数据库名

            MysqlInfo() 
            {
                server.clear();
                user.clear();
                password.clear();
                database.clear();
            }
        };
        MysqlInfo serverdb;

        struct serverInfo
        {
            char ip[MAX_NAMESIZE];
            int port;

            serverInfo() {bzero(this, sizeof(*this));}
            const serverInfo &operator=(const serverInfo &s)
            {
                bcopy(&s, this, sizeof(*this));
                return *this;
            }
        };

        std::map<std::string, std::map<std::string, serverInfo> > _serverInfoList;
        typedef std::map<std::string, std::map<std::string, serverInfo> >::iterator _serverinfo_type_iter;
        typedef std::map<std::string, serverInfo>::iterator _serverinfo_iter;
        UInt32 zoneOpenTime;
};
