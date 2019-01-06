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
    ~ZoneListenThread() {
    }

    bool accept(int sockfd, sockaddr_in &addr);

    ZoneServer *pServer;
};

// 管理服务器间连接
class ServerClient
{
    friend class ZoneServer;
public:
    ServerClient() {
        np = NULL;
        _heartbeat = SERVER_HEARTBEAT;
    }
    ~ServerClient() {
    }

    bool sendCmd(const void *cmd, unsigned short len) {
        if (cmd && len && np)
            return np->sendCmd(cmd, len);
        return false;
    }

    bool getCmd(unsigned char *&cmd, unsigned short &len) {
        if (np)
            return np->getCmd(cmd, len);
        return false;
    }
    bool popCmd() {
        if (np)
            return np->popCmd();
        return false;
    }
    void put(unsigned char *cmd, unsigned short len) {
        if (np)
            np->put(cmd, len);
    }
    inline bool get(unsigned char *&cmd, unsigned short &len) {
        if (np)
            return np->get(cmd, len);
        return false;
    }
    void pop() {
        if (np)
            np->pop();
    }
    xNetProcessor *getNp() {
        return np;
    }
    const char* getName() {
        if (np) return np->name;
        return "";
    }
    bool check() {
        _heartbeat--;
        if (_heartbeat <= 0)
            return false;
        return true;
    }
    void reset() {
        _heartbeat = SERVER_HEARTBEAT;
    }
    xNetProcessor *np;
private:

    int _heartbeat;
};

//MYSQL信息
struct MysqlInfoST
{
    std::string server;//MYSQL地址
    std::string user;//用户
    std::string password;//密码
    std::string database;//数据库名

    MysqlInfoST() {
        server.clear();
        user.clear();
        password.clear();
        database.clear();
    }
};

// 服务器信息
struct ServerInfoST
{
    int servertypeid = 0;
    char ip[MAX_NAMESIZE] = {0};
    int port = -1;

    ServerInfoST() {
        bzero(this, sizeof(*this));
    }
    const ServerInfoST &operator=(const ServerInfoST &s) {
        bcopy(&s, this, sizeof(*this));
        return *this;
    }
};


typedef std::map<UInt32, std::string> ZoneMap;
typedef std::map<xNetProcessor *, time_t>::iterator VerifyMapIt;

typedef std::map<std::string, std::map<std::string, ServerClient *> >::iterator ServerTypeMapIt;
typedef std::map<std::string, ServerClient *>::iterator ServerMapIt;

typedef std::map<std::string, std::map<std::string, ServerInfoST> >::iterator ServerinfoTypeMapIt;
typedef std::map<std::string, ServerInfoST>::iterator ServerinfoMapIt;



//分区服务器
class ZoneServer : public xServer
{
    friend class ZoneListenThread;
public:
    ZoneServer(const char *t, const char *n);
    ~ZoneServer();

public:
    bool loadZoneInfo();
    bool loadServerList();

    bool checkConnectedServer(std::string t, std::string n = "");
    void setConnectServerType(std::string t);
    uint32 getConnectServerTypeId(std::string t);
    ServerClient *getConnectedServer(std::string t, std::string n = "");
    bool getConnectedServer(std::string t, std::string n, std::list<ServerClient *> &list);

    virtual bool verifyServer(xNetProcessor *np, const char *t, const char *n);    //主线程
    bool sendCmdToServer(void *data, unsigned short len, const char *type, const char *name = "");

public:
    bool sendCmdToServer(void *data, unsigned short len, uint32 serverTypeId);
    
public:
    bool sendCmdToReg(void *data, unsigned short len);
    bool sendCmdToLogin(void *data, unsigned short len);
    bool sendCmdToTransfer(void *data, unsigned short len);
    bool sendCmdToAllianceC(void *data, unsigned short len);
    bool sendCmdToAlliance(void *data, unsigned short len);
    bool sendCmdToFriendC(void *data, unsigned short len);
    bool sendCmdToFriend(void *data, unsigned short len);
    bool sendCmdToWorld(void *data, unsigned short len);
    bool sendCmdToGame(void *data, unsigned short len);

public:
    char *GetServerIP();
    int getServerPort() const {
        return m_n32ServerPort;
    }

    UInt32 getZoneOpenTime()const {
        return m_u32ZoneOpenTime;
    }

    UInt32 getPlatformID()const {
        return m_u32PlatformId;
    }
    const char *getPlatformName()const {
        return m_szPlatformName;
    }
    UInt32 getZoneID()const {
        return m_u32ZoneID;
    }
    const char *getZoneName()const {
        return m_szZoneName;
    }

    bool getFullName(std::string &dest, std::string src, UInt32 zoneid);
    std::string getFullName(std::string src, UInt32 zoneid);
    bool getShortName(std::string &dest, std::string str);
    std::string getShortName(std::string src);

protected:
    virtual bool rInit() {
        return true;
    }
    virtual bool loadConfig() {
        return true;
    }
    virtual bool init() {
        return true;
    }
    virtual void v_final();
    bool startListen(int port);
    virtual bool accept(int sockfd, sockaddr_in &addr);
    virtual bool closeConn(xNetProcessor *np);
    virtual xTask *newTask(int sockfd);
    virtual xClient *newClient(int sockfd);

public:
    bool v_serverCallback();

    virtual bool checkConnect() = 0;//if all connected, server start ok

    virtual void sendVerify(xNetProcessor *, bool ret);
    virtual bool closeServer(xNetProcessor *);
    virtual void v_closeServer(xNetProcessor *) = 0;

public:
    bool connectServerByType(std::string t);
    bool processNetwork();
    bool processCmd(string serverName, xNetProcessor *np, unsigned char *buf, unsigned short len);

    virtual bool v_processCmd(string serverName, xNetProcessor *np, unsigned char *buf, unsigned short len) = 0;

    virtual bool runAction();

public:
    void setVerifyList(xNetProcessor *t, time_t);
    void removeVerifyList(xNetProcessor *task);
    bool inVerifyList(xNetProcessor *task);
    void verifyOk(xNetProcessor *task);
    xNetProcessor *getNpByIPAndPort(UInt32 ip, UInt32 port);
    virtual void v_verifyOk(xNetProcessor *task) {}

private:
    char            m_szServerType[MAX_NAMESIZE] = {0};//server type
    bool            m_bInit = false;

public:
    UInt32          m_u32PlatformId = 0;
    char            m_szPlatformName[MAX_NAMESIZE];
    UInt32          m_u32ZoneID = 0;
    uint32          m_serverTypeId = 0;
    char            m_szZoneName[MAX_NAMESIZE];
    ZoneMap         m_mapZone;

public:
    int             m_n32ServerPort = 0;
    ListenThread   *m_pListener = nullptr;

    xTime           m_stCheckConnectTimer;
    xTime           m_stHeartBeatTimer;

public:
    MysqlInfoST     m_stServerDB;
    UInt32          m_u32ZoneOpenTime;
    pthread_rwlock_t                    m_stVerifylistCritical;
    std::map<xNetProcessor *, time_t>   m_mapVerify;//task:time

    std::map<std::string, std::map<std::string, ServerInfoST> >     m_mapServerInfoList;
    std::map<std::string, std::map<std::string, ServerClient *> >   m_mapServerList;    //remote type:name:task
    std::map<uint32, std::map<std::string, ServerClient *> >        m_mapServerIdList;  //remote type:name:task

#ifndef _WINDOWS
    int                 m_n32ServerEpfd;//epfd
    struct epoll_event  m_szServerEv[MAX_SERVER_EVENT];
#else
    int                 m_n32ServerSefd;//epfd
    SOCKET              m_n32MinFD;
    SOCKET              m_n32MaxFD;
    uint32              m_n32FDSize;
    //网络数据  
    enum
    {
        SELECT_BAK = 0,    //  当前系统中拥有的完整句柄数据   
        SELECT_USE = 1,    //  用于select调用的句柄数据  
        SELECT_MAX = 2, //  结构使用数量  
    };
    fd_set              m_arrReadFDs[SELECT_MAX];
    fd_set              m_arrWriteFDs[SELECT_MAX];
    fd_set              m_arrExceptFDs[SELECT_MAX];
    timeval             m_arrTimeout[SELECT_MAX];

    map<int, xNetProcessor *> m_mapClient;

    void            addSocket(xNetProcessor *t);
    void            removeSocket(xNetProcessor *t);
#endif
};
