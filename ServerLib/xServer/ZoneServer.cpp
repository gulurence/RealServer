#include "ZoneServer.h"
#include "xXMLParser.h"
#include "xNetProcessor.h"
#include "xClient.h"
#include "xTask.h"
#ifndef _WINDOWS
#include <arpa/inet.h>
#endif
#include "SystemCmd.h"
#include <errno.h>
#include <iostream>

static dbCol ServerInfoCol[] =
{
    {"zoneid", DBDATA_TYPE_UInt32, sizeof(UInt32)},
    {"servertypeid", DBDATA_TYPE_UInt32, sizeof(UInt32)},
    {"servertype", DBDATA_TYPE_CHAR, MAX_NAMESIZE},
    {"servername", DBDATA_TYPE_CHAR, MAX_NAMESIZE},
    {"ip", DBDATA_TYPE_CHAR, MAX_NAMESIZE},
    {"port", DBDATA_TYPE_UInt32, sizeof(UInt32)},
    {"extip", DBDATA_TYPE_CHAR, MAX_NAMESIZE},
    {"extport", DBDATA_TYPE_UInt32, sizeof(UInt32)},
    {"NULL", 0, 0},
};

struct ServerInfo
{
    UInt32 zoneID=0;
    UInt32 serverTypeId=0;
    char servertype[MAX_NAMESIZE] = {0};
    char servername[MAX_NAMESIZE] = {0};
    char ip[MAX_NAMESIZE] = {0};
    UInt32 port=0;
    char extip[MAX_NAMESIZE] = {0};
    UInt32 extport=0;

    ServerInfo() {
        bzero(this, sizeof(*this));
    }
}__attribute__((packed));

ZoneListenThread::ZoneListenThread(const char *n, int p, ZoneServer *ps)
    :ListenThread(n, p), pServer(ps) {
}

bool ZoneListenThread::accept(int sockfd, sockaddr_in &addr) {
    return pServer->accept(sockfd, addr);
}

ZoneServer::ZoneServer(const char *t, const char *n) : xServer(n) {
    bzero(m_szServerType, sizeof(m_szServerType));
    strncpy(m_szServerType, t, MAX_NAMESIZE);

    int ret = pthread_rwlock_init(&m_stVerifylistCritical, 0);
    if (0 != ret)
        XERR("初始化读写锁失败 ret=%d", ret);

    m_n32ServerPort = 0;
    m_bInit = 0;
    m_pListener = 0;

#ifndef _WINDOWS
    m_n32ServerEpfd = 0;
    m_n32ServerEpfd = epoll_create(256);
    if (m_n32ServerEpfd < 0) {
        XERR("[%s]epoll_create() failed %d", serverName, m_n32ServerEpfd);
        m_n32ServerEpfd = 0;
    }
    else
        XLOG("[%s]create server epfd %d", serverName, m_n32ServerEpfd);
#else
    FD_ZERO(&m_arrReadFDs[SELECT_BAK]);
    FD_ZERO(&m_arrWriteFDs[SELECT_BAK]);
    FD_ZERO(&m_arrExceptFDs[SELECT_BAK]);
    m_arrTimeout[SELECT_BAK].tv_sec = 0;
    m_arrTimeout[SELECT_BAK].tv_usec = 100;
    m_n32MinFD = m_n32MaxFD = INVALID_SOCKET;
    m_n32FDSize = 0;
#endif

    m_u32ZoneID = 0;
    bzero(m_szZoneName, sizeof(m_szZoneName));
    m_u32ZoneOpenTime = 0;
    m_u32PlatformId = 0;
    bzero(m_szPlatformName, sizeof(m_szPlatformName));

    setServerState(xServer::SERVER_CREATE);
}

ZoneServer::~ZoneServer() {
    pthread_rwlock_destroy(&m_stVerifylistCritical);
}

bool ZoneServer::loadServerList() {
    bool ret = false;

    DBConn dbConn;
    if (!dbConn.init(m_stServerDB.server.c_str(), m_stServerDB.user.c_str(), m_stServerDB.password.c_str(), m_stServerDB.database.c_str())) {
        XERR("[加载服务器列表],初始化数据库连接失败");
        return false;
    }

    ServerInfo *result = NULL;

    char where[128];
    bzero(where, sizeof(where));
    snprintf(where, sizeof(where), "ZONEID=%u", m_u32ZoneID);

    UInt64 num = dbConn.exeSelect("serverlist", (dbCol *)ServerInfoCol, (unsigned char**)&result, where, NULL);
    if (UInt64_MAX != num) {
        UInt32 nlen = strlen(serverName), tlen = strlen(m_szServerType);
        for (UInt32 i = 0; i < num; i++) {
            ServerInfoST s;
            s.servertypeid = result[i].serverTypeId;
            strncpy(s.ip, result[i].ip, MAX_NAMESIZE);
            s.port = result[i].port;
            int tret = strncmp(result[i].servertype, m_szServerType, tlen), nret = strncmp(result[i].servername, serverName, nlen);
            if (0 == tret
                && 0 == nret && s.port > 0) {
                m_n32ServerPort = s.port;
                m_n32ServerTypeId = s.servertypeid;
                XLOG("[%s],服务器类型:%s 服务器名称:%s", serverName, result[i].servertype, result[i].servername);
                ret = true;
            }

            m_mapServerInfoList[result[i].servertype][result[i].servername] = s;
            XLOG("[%s],加载服务器列表,%s,%s,%s,%u", serverName, result[i].servertype, result[i].servername, result[i].ip, s.port);
        }
    }
    SAFE_DELETE_VEC(result);

    return ret;
}

bool ZoneServer::loadZoneInfo() {
    xXMLParser p;
    if (!p.parseDoc("config.xml")) {
        XERR("[%s],打开配置文件 config.xml 失败", serverName);
        return false;
    }

    xmlNodePtr root = p.getRoot();
    if (!root) {
        XERR("[%s],配置文件 config.xml 没有root节点", serverName);
        return false;
    }

    xmlNodePtr pfNode = p.getNodeByPath("Config/PlatformInfo");
    if (!pfNode) {
        XERR("[%s] 找不到节点 PlatformInfo", serverName);
        return false;
    }
    std::string pfName;
    if ((!p.getPropValue(pfNode, "name", &pfName))
        || !p.getPropValue(pfNode, "id", &m_u32PlatformId)) {
        XERR("[%s] 加载平台信息失败", serverName);
        return false;
    }
    strncpy(m_szPlatformName, pfName.c_str(), MAX_NAMESIZE - 1);
    XLOG("[%s],加载 %s(%u) 平台信息", serverName, m_szPlatformName, m_u32PlatformId);

    xmlNodePtr zoneNode = p.getChild(root, "ZoneInfo");
    if (!zoneNode) {
        XERR("[%s] 找不到节点 ZoneInfo", serverName);
        return false;
    }
    while (zoneNode) {
        UInt32 zid = 0;
        std::string zName;
        std::string zonetime;
        if ((!p.getPropValue(zoneNode, "name", &zName)) || !p.getPropValue(zoneNode, "id", &zid)
            || !p.getPropStr(zoneNode, "zoneopentime", zonetime)) {
            XERR("[%s] 加载区信息失败 ", serverName);
            return false;
        }
        if (m_u32ZoneID == 0) {
            m_u32ZoneID = zid;
            strncpy(m_szZoneName, zName.c_str(), MAX_NAMESIZE - 1);
            if (!zonetime.empty())
                parseTime(zonetime.c_str(), m_u32ZoneOpenTime);
        }
        m_mapZone[zid] = zName;

        XLOG("[%s],加载 %s(%u) 区信息,开区时间:%s(%u)", serverName, m_szZoneName, m_u32ZoneID, zonetime.c_str(), m_u32ZoneOpenTime);

        zoneNode = p.getNext(zoneNode, "ZoneInfo");
    }

    xmlNodePtr serverdbNode = p.getNodeByPath("Config/ServerList/mysql");
    if (serverdbNode) {
        p.getPropStr(serverdbNode, "server", m_stServerDB.server);
        p.getPropStr(serverdbNode, "user", m_stServerDB.user);
        p.getPropStr(serverdbNode, "password", m_stServerDB.password);
        p.getPropStr(serverdbNode, "database", m_stServerDB.database);
        XLOG("[%s],加载SERVERLIST信息 %s,%s,%s,%s", serverName, m_stServerDB.server.c_str(), m_stServerDB.user.c_str(), m_stServerDB.password.c_str(), m_stServerDB.database.c_str());
    }
    else {
        XERR("[%s] 找不到节点 ServerList/mysql", serverName);
        return false;
    }

    return true;
}

void ZoneServer::v_final()        //主线程
{
    if (m_pListener) {
        XLOG("[%s]listener server_stop", serverName);
        m_pListener->thread_stop();
        SAFE_DELETE(m_pListener);
    }

    ServerMapIt tmp_server;
    for (ServerTypeMapIt it = m_mapServerList.begin(); it != m_mapServerList.end(); it++) {
        for (ServerMapIt sub_it = it->second.begin(); sub_it != it->second.end(); ) {
            tmp_server = sub_it++;
            closeServer(tmp_server->second->np);
        }
    }
    
    m_mapServerList.clear();
    m_mapServerIdList.clear();

#ifndef _WINDOWS
    if (m_n32ServerEpfd)
        SAFE_CLOSE_SOCKET(m_n32ServerEpfd);
#else
    if (m_n32ServerSefd)
        SAFE_CLOSE_SOCKET(m_n32ServerSefd);
#endif
}

bool ZoneServer::startListen(int port) {
    if (!port) {
        XERR("[%s]监听端口错误", serverName);
        return false;
    }

    m_pListener = new ZoneListenThread("监听", port, this);
    if (!m_pListener || !m_pListener->thread_start()) {
        XERR("[%s]start listener failed", serverName);
        if (m_pListener) SAFE_DELETE(m_pListener);
        return false;
    }
    return true;
}

xTask *ZoneServer::newTask(int sockfd) {
    return new xTask(sockfd);
}

xClient *ZoneServer::newClient(int sockfd) {
    return new xClient(sockfd);
}

bool ZoneServer::accept(int sockfd, sockaddr_in &addr) {
    xTask *task = newTask(sockfd);

    setVerifyList(task, time(0));
#ifndef _WINDOWS
    task->addEpoll(m_n32ServerEpfd);
#else
    addSocket(task);
#endif

    task->m_stIp = addr.sin_addr;
    task->m_u16Port = addr.sin_port;

    XDBG("[%s]%s:%d connect, new task:%p, add verify list", serverName, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), task);

    return true;
}

#ifdef _WINDOWS
void ZoneServer::addSocket(xNetProcessor *t) {
    SOCKET fd = t->GetSock().get_fd();
    if (fd == INVALID_SOCKET)
        return;
//已经超出能够检测的网络句柄最大数
    if (m_n32FDSize >= FD_SETSIZE) {
        return;
    }

    m_n32MinFD = ((m_n32MinFD == INVALID_SOCKET) ? fd : min(fd, m_n32MinFD));
    m_n32MaxFD = ((m_n32MaxFD == INVALID_SOCKET) ? fd : max(fd, m_n32MaxFD));

    FD_SET(fd, &m_arrReadFDs[SELECT_BAK]);
    FD_SET(fd, &m_arrWriteFDs[SELECT_BAK]);
    FD_SET(fd, &m_arrExceptFDs[SELECT_BAK]);

    m_n32FDSize++;

    m_mapClient.insert(make_pair(fd, t));
}

void ZoneServer::removeSocket(xNetProcessor *t) {
    SOCKET fd = t->GetSock().get_fd();
    if (fd == INVALID_SOCKET || m_n32MinFD == INVALID_SOCKET || m_n32MaxFD == INVALID_SOCKET) {
        return;
    }

    if (fd == m_n32MinFD) {
        SOCKET &s = m_n32MinFD;
        map<int, xNetProcessor *>::iterator it = m_mapClient.begin();
        for (; it != m_mapClient.end(); ++it) {
            xNetProcessor*    pPlayer = it->second;
            if (!pPlayer)
                return;

            SOCKET temp = pPlayer->GetSock().get_fd();
            if (temp == fd)
                continue;
            if (temp == INVALID_SOCKET)
                continue;

            if (temp < s) {
                s = temp;
            }
        }

        if (m_n32MinFD == m_n32MaxFD && m_n32MaxFD == fd) {
            m_n32MinFD = m_n32MaxFD = INVALID_SOCKET;
        }
    }
    else if (fd == m_n32MaxFD) {
        SOCKET &s = m_n32MaxFD;
        map<int, xNetProcessor *>::iterator it = m_mapClient.begin();
        for (; it != m_mapClient.end(); ++it) {
            xNetProcessor*    pPlayer = it->second;
            if (!pPlayer)
                return;

            SOCKET temp = pPlayer->GetSock().get_fd();
            if (temp == fd)
                continue;
            if (temp == INVALID_SOCKET)
                continue;

            if (temp > s) {
                s = temp;
            }
        }

        if (m_n32MaxFD == m_n32MinFD && m_n32MaxFD == fd) {
            m_n32MinFD = m_n32MaxFD = INVALID_SOCKET;
        }
    }

    FD_CLR(fd, &m_arrReadFDs[SELECT_BAK]);
    FD_CLR(fd, &m_arrReadFDs[SELECT_USE]);
    FD_CLR(fd, &m_arrWriteFDs[SELECT_BAK]);
    FD_CLR(fd, &m_arrWriteFDs[SELECT_USE]);
    FD_CLR(fd, &m_arrExceptFDs[SELECT_BAK]);
    FD_CLR(fd, &m_arrExceptFDs[SELECT_USE]);

    m_n32FDSize--;

    m_mapClient.erase(fd);
}
#endif

bool ZoneServer::processNetwork() {
#ifndef _WINDOWS
    if (!m_n32ServerEpfd) return false;

    bzero(m_szServerEv, sizeof(m_szServerEv));
    int nfds = epoll_wait(m_n32ServerEpfd, m_szServerEv, MAX_SERVER_EVENT, 50);

    for (int i = 0; i < nfds; ++i) {
        xNetProcessor *np = (xNetProcessor *)m_szServerEv[i].data.ptr;
//        if (xNetProcessor::NP_ESTABLISH != np->getState())
//            continue;
    //    if(strcmp(np->m_arrcName,"SceneServer1")==0){
                //int i=0;
                //    i=1;
        //}
        if (m_szServerEv[i].events & EPOLLERR) {
            XLOG("[%s] connect error %s %p", serverName, np->m_arrcName, np);
            np->setNPState(xNetProcessor::NP_DISCONNECT);
            closeServer(np);
            continue;
        }
        else if (m_szServerEv[i].events & EPOLLIN) {
            if (!np->readCmd()) {
                XLOG("[%s] read error %s %p", serverName, np->m_arrcName, np);
                np->setNPState(xNetProcessor::NP_DISCONNECT);
                closeServer(np);
                continue;
            }

            unsigned char *cmd = 0;
            unsigned short cmdLen;
            bool process_ret = true;
            while (np->getCmd(cmd, cmdLen)) {
                process_ret = processCmd(np->m_arrcName, np, cmd, cmdLen);
                np->popCmd();
                if (!cmdLen || !process_ret) break;
            }
            if (cmdLen && !process_ret) {
                xCommand *c = (xCommand *)cmd;
                if (c) {
                    XERR("[%s] message error %s %p (%u, %u),len:%u", serverName, np->m_arrcName, np, c->cmd, c->param, cmdLen);
                    //return true;
                }
                else {
                    XERR("[%s] message error %s %p", serverName, np->m_arrcName, np);
                }

                np->setNPState(xNetProcessor::NP_DISCONNECT);
                closeServer(np);
                continue;
            }
        }
        /*
           if (server_ev[i].events & EPOLLOUT)
           {
#ifdef _DEBUG
        //XLOG("[%s] EPOLLOUT %s %p", serverName, np->m_arrcName, np);
#endif
while (np->sendCmd())
{
#ifdef _DEBUG
        //    XLOG("[%s] EPOLLOUT send, %s %p", serverName, np->m_arrcName, np);
#endif
}

}
*/
    }

#else
    if (!m_n32ServerSefd)
        return false;

    // 检查socket 状态
    {
        Sleep(10);
        if (m_n32MaxFD == INVALID_SOCKET && m_n32MinFD == INVALID_SOCKET) {
            return TRUE;
        }

        m_arrTimeout[SELECT_USE].tv_sec = m_arrTimeout[SELECT_BAK].tv_sec;
        m_arrTimeout[SELECT_USE].tv_usec = m_arrTimeout[SELECT_BAK].tv_usec;

        m_arrReadFDs[SELECT_USE] = m_arrReadFDs[SELECT_BAK];
        m_arrWriteFDs[SELECT_USE] = m_arrWriteFDs[SELECT_BAK];
        m_arrExceptFDs[SELECT_USE] = m_arrExceptFDs[SELECT_BAK];

        try {
            INT iRet = select((INT)m_n32MaxFD + 1,
                              &m_arrReadFDs[SELECT_USE],
                              &m_arrWriteFDs[SELECT_USE],
                              &m_arrExceptFDs[SELECT_USE],
                              &m_arrTimeout[SELECT_USE]);
            if (iRet == SOCKET_ERROR) {
                return false;
            }
        }
        catch (...) {
            printf("");
            return false;
        }
    }

    // 执行相应的操作
    {
        map<int, xNetProcessor *>::iterator it_temp;
        map<int, xNetProcessor *>::iterator it = m_mapClient.begin();
        for (; it != m_mapClient.end(); ) {

            it_temp = it++;

            xNetProcessor* np = it_temp->second;
            if (!np)
                return false;

            SOCKET s = np->GetSock().get_fd();
            if (s == SOCKET_ERROR)
                continue;

            if (FD_ISSET(s, &m_arrExceptFDs[SELECT_USE])) {
                XLOG("[%s] connect error %s %p", serverName, np->m_arrcName, np);
                np->setNPState(xNetProcessor::NP_DISCONNECT);
                closeServer(np);
                continue;
            }
            if (FD_ISSET(s, &m_arrReadFDs[SELECT_USE])) {
                if (!np->readCmd()) {
                    XLOG("[%s] connect error %s %p", serverName, np->m_arrcName, np);
                    np->setNPState(xNetProcessor::NP_DISCONNECT);
                    closeServer(np);
                    continue;
                }

                unsigned char *cmd = 0;
                unsigned short cmdLen;
                bool process_ret = true;
                while (np->getCmd(cmd, cmdLen)) {
                    process_ret = processCmd(np->m_arrcName, np, cmd, cmdLen);
                    np->popCmd();
                    if (!cmdLen || !process_ret) break;
                }
                if (cmdLen && !process_ret) {
                    xCommand *c = (xCommand *)cmd;
                    if (c) {
                        XERR("[%s] message error %s %p (%u, %u),len:%u", serverName, np->m_arrcName, np, c->cmd, c->param, cmdLen);
                        //return true;
                    }
                    else {
                        XERR("[%s] message error %s %p", serverName, np->m_arrcName, np);
                    }

                    np->setNPState(xNetProcessor::NP_DISCONNECT);
                    closeServer(np);
                    continue;
                }
            }
            if (FD_ISSET(s, &m_arrWriteFDs[SELECT_USE])) {

            }
        }
    }

#endif

    return true;
}

bool ZoneServer::verifyServer(xNetProcessor *np, const char *t, const char *n)    //主线程
{
    if (!np) return false;

    bool ret = true;

    uint32 serverTypeId = getConnectServerTypeId(t);
    if (0 >= serverTypeId) {
        XERR("[%s] get servertypeid error server %s %s(%p)", serverName, t, n, np);
        return false;
    }

    if (inVerifyList(np)) {
        ServerClient *ta = getConnectedServer(t, n);
        if (ta) {
            if (ta->np) {
                XERR("[%s]%s %s(%p) already in server list:%p", serverName, t, n, np, ta);
                ret = false;
            }
            else {
                if ((m_mapServerInfoList.find(t) == m_mapServerInfoList.end())
                    || (m_mapServerInfoList[t].find(n) == m_mapServerInfoList[t].end())) {
                    XERR("[%s]refuse unkown server %s %s(%p)", serverName, t, n, np);
                    ret = false;
                }
            }
        }
    }
    else {
        XERR("[%s]%s %s not in verify list %p", serverName, t, n, np);
        ret = false;
    }

    if (np->isTask()) {
        sendVerify(np, ret);
    }

    XLOG("[%s]%s %s verify %s %p", serverName, t, n, ret ? "ok" : "failed", np);
    if (ret) {
        np->setNPState(xNetProcessor::NP_ESTABLISH);

        removeVerifyList(np);

        if (m_mapServerList.find(t) != m_mapServerList.end()) {
            if (m_mapServerList[t].find(n) != m_mapServerList[t].end()) {
                
                m_mapServerList[t][n]->np = np;
                m_mapServerIdList[serverTypeId][n]->np = np;
                m_mapServerList[t][n]->reset();
                verifyOk(np);
                XLOG("[%s],add to serverList %s", serverName, t);
            }
            else
                XERR("[%s],add to serverList fail %s", serverName, t);
        }
        else
            XERR("[%s],add to serverList fail %s, %s", serverName, t, n);
    }
    //else {
    //    closeServer(np);
    //}
    return ret;
}

bool ZoneServer::runAction() {
    if (m_stCheckConnectTimer.elapse(3)) {
        if (checkConnect()) {

            if (!m_pListener)
                startListen(m_n32ServerPort);

            //wwj:与各服务器连接成功以后再初始化一次，临时用，还没有想到更好的方法
            if (!m_bInit) {
                m_bInit = true;
                return rInit();
            }
        }
    }
    return true;
}

bool ZoneServer::v_serverCallback() {
    switch (getServerState()) {
    case SERVER_CREATE:
    {
        return true;
    }
    break;
    case SERVER_INIT:
    {
        XLOG("[%s],加载区信息  ", serverName);
        if (!loadZoneInfo()) {
            server_stop();
            break;
        }

        XLOG("[%s],加载服务器列表  ", serverName);
        if (!loadServerList()) {
            XERR("[%s],加载服务器列表失败  ", serverName);
            server_stop();
            break;
        }

        XLOG("[%s],加载配置 ", serverName);
        if (!loadConfig()) {
            server_stop();
            break;
        }

        XLOG("[%s],初始化 init", serverName);
        if (!init()) {
            server_stop();
            break;
        }

        setServerState(SERVER_RUN);
        return true;
    }
    break;
    case SERVER_RUN:
    {
        if (!runAction()) {
            XERR("[%s]runAction err", serverName);
            return false;
        }
    }
    break;
    case SERVER_STOP:
    {
        if (v_stop())
            return false;
        return true;
    }
    break;
    case SERVER_FINISH:
    {
        return false;
    }
    break;
    default:
        break;
    }
    return true;
}

bool ZoneServer::checkConnectedServer(std::string t, std::string n)    //主线程
{
    if ((t == "") || (m_mapServerList.find(t) == m_mapServerList.end())) 
        return false;

    if ((n != "") && (m_mapServerList[t].find(n) == m_mapServerList[t].end())) 
        return false;

    if (m_mapServerList.find(t) != m_mapServerList.end()) {
        if (n != "") 
            return (m_mapServerList[t].find(n) != m_mapServerList[t].end() && m_mapServerList[t][n]->np);

        for (ServerMapIt it = m_mapServerList[t].begin(); it != m_mapServerList[t].end(); it++)
            if (NULL == it->second->np) return false;
    }
    else
        return false;

    return true;
}

void ZoneServer::setConnectServerType(std::string t)    //主线程
{
    auto it = m_mapServerInfoList.find(t);
    if ((t == "") || (it == m_mapServerInfoList.end()))
        return;

    for (ServerinfoMapIt s_it = m_mapServerInfoList[t].begin(); s_it != m_mapServerInfoList[t].end(); s_it++) {
        if (getConnectedServer(t, s_it->first))
            continue;

        m_mapServerList[t][s_it->first] = new ServerClient();

        // 初始化ID对应关系列表
        uint32 serverTypeId = getConnectServerTypeId(t);
        m_mapServerIdList[serverTypeId][s_it->first] = m_mapServerList[t][s_it->first];

        XDBG("[%s],设置要连接的服务器:%s,%s,%p", serverName, t.c_str(), s_it->first.c_str(), m_mapServerList[t][s_it->first]);
    }
}

bool ZoneServer::connectServerByType(std::string t) {
    std::map<std::string, std::map<std::string, ServerInfoST> > tempList;

    ServerinfoTypeMapIt type_it;
    ServerinfoMapIt serverinfo_it;

    if (t != "") {
        type_it = m_mapServerInfoList.find(t);
        if (type_it == m_mapServerInfoList.end())
            return false;
        tempList.insert(*type_it);
    }
    else
        tempList = m_mapServerInfoList;

    bool ret = true;
    for (type_it = tempList.begin(); type_it != tempList.end(); type_it++) {
        for (ServerinfoMapIt s_it = type_it->second.begin(); s_it != type_it->second.end(); s_it++) {
            bool retTemp = false;
            do {
                ServerClient *sc = getConnectedServer(type_it->first, s_it->first);
                // 已经成功的不过处理
                if (sc && sc->getNp()) {
                    retTemp = true;
                    break;
                }

                int s = socket(AF_INET, SOCK_STREAM, 0);
                if (s < 0) {
                    XERR("[%s]connectServerByType %s socket() failed %d", serverName, t.c_str(), s);
                    break;
                }

                xClient *c = newClient(s);
                if (!c) {
                    XERR("[%s]connectServerByType %s newClient failed", serverName, t.c_str());
#ifndef _WINDOWS
                    close(s);
#else
                    closesocket(s);
#endif
                    break;
                }

                c->setName(s_it->first.c_str());

                if (!c->connect(s_it->second.ip, s_it->second.port)) {
                    XERR("[%s]connectServerByType %s failed %s:%d,%s", serverName, t.c_str(), s_it->second.ip, s_it->second.port, strerror(errno));
                    SAFE_DELETE(c);
#ifndef _WINDOWS
                    close(s);
#else
                    closesocket(s);
#endif
                    break;
                }

                setVerifyList(c, time(0));

#ifndef _WINDOWS
                c->addEpoll(m_n32ServerEpfd);
#else
                addSocket(c);
#endif

                XDBG("[%s]connectServerByType %s ok %s:%d %p", serverName, t.c_str(), s_it->second.ip, s_it->second.port, c);

                sendVerify(c, true);

                retTemp = true;

            } while (false);

            if (!retTemp && ret) {
                ret = false;
            }
        }
    }

    return ret;
}

void ZoneServer::sendVerify(xNetProcessor *np, bool ret) {
    if (!np) return;

    VerifyConnSystemCmd send;
    send.ret = ret ? 1 : 0;
    strncpy(send.type, m_szServerType, MAX_NAMESIZE);
    strncpy(send.name, serverName, MAX_NAMESIZE);
    send.zoneID = this->m_u32ZoneID;
    np->sendCmd(&send, sizeof(send));
}

bool ZoneServer::closeServer(xNetProcessor *np) {

    if (nullptr == np) {
        return false;
    }

    for (ServerTypeMapIt it = m_mapServerList.begin(); it != m_mapServerList.end(); it++) {
        for (ServerMapIt sub_it = it->second.begin(); sub_it != it->second.end(); sub_it++) {
            if (sub_it->second->np == np) {
                sub_it->second->np = NULL;
                v_closeServer(np);
                closeConn(np);
                return true;
            }
        }
    }

    v_closeServer(np);
    removeVerifyList(np);
    closeConn(np);

    return false;
}

bool ZoneServer::closeConn(xNetProcessor *np) {
#ifdef _WINDOWS
    removeSocket(np);
#endif
    return xServer::closeConn(np);
}

uint32 ZoneServer::getConnectServerTypeId(std::string t) {
    auto it = m_mapServerInfoList.find(t);
    if (it == m_mapServerInfoList.end()) {
        return 0;
    }

    if (it->second.size() <= 0) {
        return 0;
    }

    auto it_temp = it->second.begin();
    return it_temp->second.servertypeid;
}

int32 ZoneServer::getThisServerTypeId() {
    return m_n32ServerTypeId;
}

ServerClient *ZoneServer::getConnectedServer(std::string t, std::string n) {
    std::map<std::string, std::map<std::string, ServerClient *> > tempList;

    ServerTypeMapIt type_it;
    ServerMapIt task_it;

    if (t != "") {
        type_it = m_mapServerList.find(t);
        if (type_it == m_mapServerList.end()) 
            return 0;

        tempList.insert(*type_it);
    }
    else
        tempList = m_mapServerList;

    for (type_it = tempList.begin(); type_it != tempList.end(); type_it++) {
        if (n != "")
            task_it = type_it->second.find(n);
        else
            task_it = type_it->second.begin();

        if (task_it == type_it->second.end())
            return 0;

        return task_it->second;
    }
    return 0;
}

bool ZoneServer::getConnectedServer(std::string t, std::string n, std::list<ServerClient *> &list) {
    std::map<std::string, std::map<std::string, ServerClient *> > tempList;

    ServerTypeMapIt type_it;
    ServerMapIt task_it;

    if (t != "") {
        type_it = m_mapServerList.find(t);
        if (type_it == m_mapServerList.end()) return 0;
        tempList.insert(*type_it);
    }
    else
        tempList = m_mapServerList;

    for (type_it = tempList.begin(); type_it != tempList.end(); type_it++) {
        if (n != "") {
            task_it = type_it->second.find(n);
            if (task_it != type_it->second.end())
                list.push_back(task_it->second);
        }
        else {
            for (task_it = type_it->second.begin(); task_it != type_it->second.end(); task_it++) {
                list.push_back(task_it->second);
            }
        }
    }
    return !list.empty();
}

bool ZoneServer::sendCmdToServer(void *data, unsigned short len, const char *type, const char *name) {
    std::list<ServerClient *> list;
    if (!getConnectedServer(type, name != NULL ? name : "", list))
        return false;

    //XDBG("send Cmd ToServer[%s],serverNum[%u]",type,list.size());
    for (auto &it: list)
        it->sendCmd(data, len);

    return true;
}

bool ZoneServer::sendCmdToServer(void *data, unsigned short len, uint32 serverTypeId) {

    /*
    
    发Ts接口需要包装         发Ts接口不需要包装了        直接发给目标或者集群上级节点          发给集群叶子          集群叶子 
    LoginServer        ->    GameServer            ->    *TransferServer*                ->    FriendC         ->    Friend1

    *TransferServer* -> RegServer
    
    */

    // 发给自己服务器的 先不处理直接返回
    if (getThisServerTypeId() == xServer::SERVER_TID_TRANSFER) {

        XERR("ZoneServer::sendCmdToServer [targetServerTypeId:%d,cmd:%d,param:%d] error !!!", serverTypeId, ((xCommand*)data)->cmd, ((xCommand*)data)->param);

        return true;
    }

    xCommand *pCmd = (xCommand *)data;

    // 如果跨Server进程 这里需要封装一次
    auto it = m_mapServerIdList.find(serverTypeId);
    if (it == m_mapServerIdList.end()) {

        // 如果是集群叶子服务器 则 发给上一级的管理节点
        // 进行一次广播过滤
        if (getThisServerTypeId() == xServer::SERVER_TID_TRANSFER && xServer::SERVER_TID_NODE_NUM <= serverTypeId) {

            // 找到上级节点直接返回 否则广播
            bool bFindParentNode = false;
            // 发给集群上级节点
            for (auto it_typeS: m_mapServerIdList) {
                if (serverTypeId / xServer::SERVER_TID_NODE_NUM == it_typeS.first) {
                    for (auto &it_temp : it_typeS.second) {
                        it_temp.second->sendCmd(data, len);
                    }

                    bFindParentNode = true;
                }
            }

            if(bFindParentNode)
                return true;
        }

        // 不是包装过的 包装成转发消息
        if (SYSTEM_CMD != pCmd->cmd && SERVER_TRANSFER_SYSCMD != pCmd->param) {

            static ServerTransferSysCmd sTransferSysCmd;
            sTransferSysCmd.targetServerTypsId = serverTypeId;
            memcpy(sTransferSysCmd.arrData, data, len);
            sTransferSysCmd.cmdLen = len;
            if (!sendCmdToTransfer(&sTransferSysCmd, sTransferSysCmd.getLen())) {
                XERR("ZoneServer::sendCmdToServer sendCmdToTransfer [targetServerTypeId:%d,cmd:%d,param:%d] error !!!", serverTypeId, ((xCommand*)data)->cmd, ((xCommand*)data)->param);
            }

        } else {

            // 包装过的直接转发
            if (!sendCmdToTransfer(data, len)) {
                XERR("ZoneServer::sendCmdToServer sendCmdToTransfer [targetServerTypeId:%d,cmd:%d,param:%d] error !!!", serverTypeId, ((xCommand*)data)->cmd, ((xCommand*)data)->param);
            }
        }

    } else {

        // 找到目标节点直接发送
        for (auto &it_temp:it->second) {
            if (it_temp.second) {
                it_temp.second->sendCmd(data, len);
            }
        }
    }

    return true;
}

bool ZoneServer::sendCmdToReg(void *data, unsigned short len) {
    return sendCmdToServer(data, len, xServer::SERVER_TID_REG);
}

bool ZoneServer::sendCmdToLogin(void *data, unsigned short len) {
    return sendCmdToServer(data, len, xServer::SERVER_TID_LOGIN);
}

bool ZoneServer::sendCmdToTransfer(void *data, unsigned short len) {
    return sendCmdToServer(data, len, xServer::SERVER_TID_TRANSFER);
}

bool ZoneServer::sendCmdToAllianceC(void *data, unsigned short len) {
    return sendCmdToServer(data, len, xServer::SERVER_TID_ALLIANCE_C);
}

bool ZoneServer::sendCmdToAlliance(void *data, unsigned short len) {
    return sendCmdToServer(data, len, xServer::SERVER_TID_ALLIANCE);
}

bool ZoneServer::sendCmdToFriendC(void *data, unsigned short len) {
    return sendCmdToServer(data, len, xServer::SERVER_TID_FRIEND_C);
}

bool ZoneServer::sendCmdToFriend(void *data, unsigned short len) {
    return sendCmdToServer(data, len, xServer::SERVER_TID_FRIEND);
}

bool ZoneServer::sendCmdToWorld(void *data, unsigned short len) {
    return sendCmdToServer(data, len, xServer::SERVER_TID_WORLD);
}

bool ZoneServer::sendCmdToGame(void *data, unsigned short len) {
    return sendCmdToServer(data, len, xServer::SERVER_TID_GAME);
}

bool ZoneServer::processCmd(string serverName, xNetProcessor *np, unsigned char *buf, unsigned short len) {

    if (!np || !buf || !len || len < sizeof(xCommand)) return false;

    xCommand *cmd = (xCommand *)buf;

    if (cmd->is(SYSTEM_CMD)) {
        switch (cmd->param) {
        case VERIFY_CONN_SYSCMD:
        {

            VerifyConnSystemCmd *rev = (VerifyConnSystemCmd *)cmd;
            if (!np->isClient()) {
                np->setName(rev->name);//need
                np->m_u32ZoneId = rev->zoneID;
            }
            bool vRet = 0 != rev->ret;
            if (vRet)
                vRet = verifyServer(np, rev->type, rev->name);

            return vRet;
        }
        break;
        case SERVER_CLOSE_SYSCMD:
        {

            printf("start--setServerState name:%s,struts:%d\n", this->serverName, server_state);
            if (strncmp(this->m_szServerType, "SceneServer", MAX_NAMESIZE) == 0)
                setServerState(SERVER_SAVE);
            else
                setServerState(SERVER_STOP);
            printf("end----setServerState name:%s,struts:%d\n", this->serverName, server_state);
        }break;
        default:
            break;
        }
    }

    v_processCmd(serverName, np, buf, len);
    //np->put(buf, len);
    return true;
}

char *ZoneServer::GetServerIP() {
    ServerinfoTypeMapIt type_it = m_mapServerInfoList.find(m_szServerType);
    if (type_it != m_mapServerInfoList.end()) {
        ServerinfoMapIt name_it = type_it->second.find(serverName);
        if (name_it != type_it->second.end())
            return name_it->second.ip;
    }
    return NULL;
}

void ZoneServer::setVerifyList(xNetProcessor *task, time_t time) {
    if (!task) return;

    XDBG("[verify_list],Add:%p", task);

    pthread_rwlock_wrlock(&m_stVerifylistCritical);
    m_mapVerify[task] = time;
    pthread_rwlock_unlock(&m_stVerifylistCritical);
}

void ZoneServer::removeVerifyList(xNetProcessor *task) {
    if (!task) 
        return;

    XDBG("[verify_list],removeVerifyList:%p", task);

    pthread_rwlock_wrlock(&m_stVerifylistCritical);
    m_mapVerify.erase(task);
    pthread_rwlock_unlock(&m_stVerifylistCritical);
}

bool ZoneServer::inVerifyList(xNetProcessor *task) {
    if (!task) return false;

    bool retcode = false;

    pthread_rwlock_wrlock(&m_stVerifylistCritical);
    retcode = (m_mapVerify.find(task) != m_mapVerify.end());
    pthread_rwlock_unlock(&m_stVerifylistCritical);

    return retcode;
}

xNetProcessor *ZoneServer::getNpByIPAndPort(UInt32 ip, UInt32 port) {
    pthread_rwlock_wrlock(&m_stVerifylistCritical);
    for (VerifyMapIt iter = m_mapVerify.begin(); iter != m_mapVerify.end(); ++iter) {
        if (iter->first && *((UInt32 *)&iter->first->m_stIp) == ip && ((iter->first))->m_u16Port == port)
            return iter->first;
    }
    pthread_rwlock_unlock(&m_stVerifylistCritical);

    return NULL;
}

void ZoneServer::verifyOk(xNetProcessor *task) {
    v_verifyOk(task);
}

bool ZoneServer::getFullName(std::string &dest, std::string src, UInt32 zoneid) {
    dest = src;

    ZoneMap::iterator iter = m_mapZone.find(zoneid);
    if (iter == m_mapZone.end())
        return false;

    std::string tmp = "." + iter->second;
    if (dest.find(tmp) == std::string::npos)
        dest += tmp;

    return true;
}

std::string ZoneServer::getFullName(std::string src, UInt32 zoneid) {
    std::string ret;
    getFullName(ret, src, zoneid);
    return ret;
}

bool ZoneServer::getShortName(std::string &dest, std::string src) {
    dest = src;

    std::string tmp = ".S";
    std::string::size_type i = 0;
    i = dest.find(tmp);
    if (i != std::string::npos)
        dest = dest.substr(0, i);

    return true;
}

std::string ZoneServer::getShortName(std::string src) {
    std::string ret;
    getShortName(ret, src);
    return ret;
}

