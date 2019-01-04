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
    {"zoneiD", DBDATA_TYPE_UInt32, sizeof(UInt32)},
    {"servertype", DBDATA_TYPE_CHAR, MAX_NAMESIZE},
    {"servername", DBDATA_TYPE_CHAR, MAX_NAMESIZE},
    {"ip", DBDATA_TYPE_CHAR, MAX_NAMESIZE},
    {"port", DBDATA_TYPE_UInt32, sizeof(UInt32)},
    {"extip", DBDATA_TYPE_CHAR, MAX_NAMESIZE},
    {"extpoRT", DBDATA_TYPE_UInt32, sizeof(UInt32)},
    {"NULL", 0, 0},
};

struct ServerInfo
{
    UInt32 zoneID;
    char servertype[MAX_NAMESIZE];
    char servername[MAX_NAMESIZE];
    char ip[MAX_NAMESIZE];
    UInt32 port;
    char extip[MAX_NAMESIZE];
    UInt32 extport;
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
    bzero(serverType, sizeof(serverType));
    strncpy(serverType, t, MAX_NAMESIZE);

    int ret = pthread_rwlock_init(&verifylist_critical, 0);
    if (0 != ret)
        XERR("初始化读写锁失败 ret=%d", ret);

    server_port = 0;
    r_init = 0;
    listener = 0;

#ifndef _WINDOWS
    server_epfd = 0;
    server_epfd = epoll_create(256);
    if (server_epfd < 0) {
        XERR("[%s]epoll_create() failed %d", serverName, server_epfd);
        server_epfd = 0;
    }
    else
        XLOG("[%s]create server epfd %d", serverName, server_epfd);
#else
    FD_ZERO(&m_ReadFDs[SELECT_BAK]);
    FD_ZERO(&m_WriteFDs[SELECT_BAK]);
    FD_ZERO(&m_ExceptFDs[SELECT_BAK]);
    m_Timeout[SELECT_BAK].tv_sec = 0;
    m_Timeout[SELECT_BAK].tv_usec = 100;
    m_MinFD = m_MaxFD = INVALID_SOCKET;
    m_nFDSize = 0;
#endif

    zoneID = 0;
    bzero(zoneName, sizeof(zoneName));
    zoneOpenTime = 0;
    platformID = 0;
    bzero(platformName, sizeof(platformName));

    setServerState(xServer::SERVER_CREATE);
}

ZoneServer::~ZoneServer() {
    pthread_rwlock_destroy(&verifylist_critical);
}

bool ZoneServer::loadServerList() {
    bool ret = false;

    DBConn dbConn;
    if (!dbConn.init(serverdb.server.c_str(), serverdb.user.c_str(), serverdb.password.c_str(), serverdb.database.c_str())) {
        XERR("[加载服务器列表],初始化数据库连接失败");
        return false;
    }

    ServerInfo *result = NULL;

    char where[128];
    bzero(where, sizeof(where));
    snprintf(where, sizeof(where), "ZONEID=%u", zoneID);

    UInt64 num = dbConn.exeSelect("serverlist", (dbCol *)ServerInfoCol, (unsigned char**)&result, where, NULL);
    if (UInt64_MAX != num) {
        UInt32 nlen = strlen(serverName), tlen = strlen(serverType);
        for (UInt32 i = 0; i < num; i++) {
            serverInfo s;
            strncpy(s.ip, result[i].ip, MAX_NAMESIZE);
            s.port = result[i].port;
            int tret = strncmp(result[i].servertype, serverType, tlen), nret = strncmp(result[i].servername, serverName, nlen);
            if (0 == tret
                && 0 == nret && s.port > 0) {
                server_port = s.port;
                XLOG("[%s],服务器类型:%s 服务器名称:%s", serverName, result[i].servertype, result[i].servername);
                ret = true;
            }

            _serverInfoList[result[i].servertype][result[i].servername] = s;
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
        || !p.getPropValue(pfNode, "id", &platformID)) {
        XERR("[%s] 加载平台信息失败", serverName);
        return false;
    }
    strncpy(platformName, pfName.c_str(), MAX_NAMESIZE - 1);
    XLOG("[%s],加载 %s(%u) 平台信息", serverName, platformName, platformID);

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
        if (zoneID == 0) {
            zoneID = zid;
            strncpy(zoneName, zName.c_str(), MAX_NAMESIZE - 1);
            if (!zonetime.empty())
                parseTime(zonetime.c_str(), zoneOpenTime);
        }
        zoneMap[zid] = zName;

        XLOG("[%s],加载 %s(%u) 区信息,开区时间:%s(%u)", serverName, zoneName, zoneID, zonetime.c_str(), zoneOpenTime);

        zoneNode = p.getNext(zoneNode, "ZoneInfo");
    }

    xmlNodePtr serverdbNode = p.getNodeByPath("Config/ServerList/mysql");
    if (serverdbNode) {
        p.getPropStr(serverdbNode, "server", serverdb.server);
        p.getPropStr(serverdbNode, "user", serverdb.user);
        p.getPropStr(serverdbNode, "password", serverdb.password);
        p.getPropStr(serverdbNode, "database", serverdb.database);
        XLOG("[%s],加载SERVERLIST信息 %s,%s,%s,%s", serverName, serverdb.server.c_str(), serverdb.user.c_str(), serverdb.password.c_str(), serverdb.database.c_str());
    }
    else {
        XERR("[%s] 找不到节点 ServerList/mysql", serverName);
        return false;
    }

    return true;
}

void ZoneServer::v_final()        //主线程
{
    if (listener) {
        XLOG("[%s]listener server_stop", serverName);
        listener->thread_stop();
        SAFE_DELETE(listener);
    }

    _server_iter tmp_server;
    for (_server_type_iter it = _serverList.begin(); it != _serverList.end(); it++)
        for (_server_iter sub_it = it->second.begin(); sub_it != it->second.end(); ) {
            tmp_server = sub_it++;
            closeServer(tmp_server->second->np);
        }
    _serverList.clear();

#ifndef _WINDOWS
    if (server_epfd)
        SAFE_CLOSE_SOCKET(server_epfd);
#else
    if (server_sefd)
        SAFE_CLOSE_SOCKET(server_sefd);
#endif
}

bool ZoneServer::startListen(int port) {
    if (!port) {
        XERR("[%s]监听端口错误", serverName);
        return false;
    }

    listener = new ZoneListenThread("监听", port, this);
    if (!listener || !listener->thread_start()) {
        XERR("[%s]start listener failed", serverName);
        if (listener) SAFE_DELETE(listener);
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
    task->addEpoll(server_epfd);
#else
    addSocket(task);
#endif

    task->ip = addr.sin_addr;
    task->port = addr.sin_port;

    XDBG("[%s]%s:%d connect, new task:%p, add verify list", serverName, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), task);

    return true;
}

#ifdef _WINDOWS
void ZoneServer::addSocket(xNetProcessor *t) {
    SOCKET fd = t->GetSock().get_fd();
    if (fd == INVALID_SOCKET)
        return;
//已经超出能够检测的网络句柄最大数
    if (m_nFDSize >= FD_SETSIZE) {
        return;
    }

    m_MinFD = ((m_MinFD == INVALID_SOCKET) ? fd : min(fd, m_MinFD));
    m_MaxFD = ((m_MaxFD == INVALID_SOCKET) ? fd : max(fd, m_MaxFD));

    FD_SET(fd, &m_ReadFDs[SELECT_BAK]);
    FD_SET(fd, &m_WriteFDs[SELECT_BAK]);
    FD_SET(fd, &m_ExceptFDs[SELECT_BAK]);

    m_nFDSize++;

    clientMap.insert(make_pair(fd, t));
}

void ZoneServer::removeSocket(xNetProcessor *t) {
    SOCKET fd = t->GetSock().get_fd();
    if (fd == INVALID_SOCKET || m_MinFD == INVALID_SOCKET || m_MaxFD == INVALID_SOCKET) {
        return;
    }

    if (fd == m_MinFD) {
        SOCKET &s = m_MinFD;
        map<int, xNetProcessor *>::iterator it = clientMap.begin();
        for (; it != clientMap.end(); ++it) {
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

        if (m_MinFD == m_MaxFD && m_MaxFD == fd) {
            m_MinFD = m_MaxFD = INVALID_SOCKET;
        }
    }
    else if (fd == m_MaxFD) {
        SOCKET &s = m_MaxFD;
        map<int, xNetProcessor *>::iterator it = clientMap.begin();
        for (; it != clientMap.end(); ++it) {
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

        if (m_MaxFD == m_MinFD && m_MaxFD == fd) {
            m_MinFD = m_MaxFD = INVALID_SOCKET;
        }
    }

    FD_CLR(fd, &m_ReadFDs[SELECT_BAK]);
    FD_CLR(fd, &m_ReadFDs[SELECT_USE]);
    FD_CLR(fd, &m_WriteFDs[SELECT_BAK]);
    FD_CLR(fd, &m_WriteFDs[SELECT_USE]);
    FD_CLR(fd, &m_ExceptFDs[SELECT_BAK]);
    FD_CLR(fd, &m_ExceptFDs[SELECT_USE]);

    m_nFDSize--;

    clientMap.erase(fd);
}
#endif

//主线程
bool ZoneServer::processNetwork() {
#ifndef _WINDOWS
    if (!server_epfd) return false;

    bzero(server_ev, sizeof(server_ev));
    int nfds = epoll_wait(server_epfd, server_ev, MAX_SERVER_EVENT, 50);

    for (int i = 0; i < nfds; ++i) {
        xNetProcessor *np = (xNetProcessor *)server_ev[i].data.ptr;
//        if (xNetProcessor::NP_ESTABLISH != np->getState())
//            continue;
    //    if(strcmp(np->name,"SceneServer1")==0){
                //int i=0;
                //    i=1;
        //}
        if (server_ev[i].events & EPOLLERR) {
            XLOG("[%s] connect error %s %p", serverName, np->name, np);
            np->setNPState(xNetProcessor::NP_DISCONNECT);
            closeServer(np);
            continue;
        }
        else if (server_ev[i].events & EPOLLIN) {
            if (!np->readCmd()) {
                XLOG("[%s] read error %s %p", serverName, np->name, np);
                np->setNPState(xNetProcessor::NP_DISCONNECT);
                closeServer(np);
                continue;
            }

            unsigned char *cmd = 0;
            unsigned short cmdLen;
            bool process_ret = true;
            while (np->getCmd(cmd, cmdLen)) {
                process_ret = processCmd(np->name, np, cmd, cmdLen);
                np->popCmd();
                if (!cmdLen || !process_ret) break;
            }
            if (cmdLen && !process_ret) {
                xCommand *c = (xCommand *)cmd;
                if (c) {
                    XERR("[%s] message error %s %p (%u, %u),len:%u", serverName, np->name, np, c->cmd, c->param, cmdLen);
                    //return true;
                }
                else {
                    XERR("[%s] message error %s %p", serverName, np->name, np);
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
        //XLOG("[%s] EPOLLOUT %s %p", serverName, np->name, np);
#endif
while (np->sendCmd())
{
#ifdef _DEBUG
        //    XLOG("[%s] EPOLLOUT send, %s %p", serverName, np->name, np);
#endif
}

}
*/
    }

#else
    if (!server_sefd)
        return false;

    // 检查socket 状态
    {
        Sleep(10);
        if (m_MaxFD == INVALID_SOCKET && m_MinFD == INVALID_SOCKET) {
            return TRUE;
        }

        m_Timeout[SELECT_USE].tv_sec = m_Timeout[SELECT_BAK].tv_sec;
        m_Timeout[SELECT_USE].tv_usec = m_Timeout[SELECT_BAK].tv_usec;

        m_ReadFDs[SELECT_USE] = m_ReadFDs[SELECT_BAK];
        m_WriteFDs[SELECT_USE] = m_WriteFDs[SELECT_BAK];
        m_ExceptFDs[SELECT_USE] = m_ExceptFDs[SELECT_BAK];

        try {
            INT iRet = select((INT)m_MaxFD + 1,
                              &m_ReadFDs[SELECT_USE],
                              &m_WriteFDs[SELECT_USE],
                              &m_ExceptFDs[SELECT_USE],
                              &m_Timeout[SELECT_USE]);
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
        map<int, xNetProcessor *>::iterator it = clientMap.begin();
        for (; it != clientMap.end(); ) {

            it_temp = it++;

            xNetProcessor* np = it_temp->second;
            if (!np)
                return false;

            SOCKET s = np->GetSock().get_fd();
            if (s == SOCKET_ERROR)
                continue;

            if (FD_ISSET(s, &m_ExceptFDs[SELECT_USE])) {
                XLOG("[%s] connect error %s %p", serverName, np->name, np);
                np->setNPState(xNetProcessor::NP_DISCONNECT);
                closeServer(np);
                continue;
            }
            if (FD_ISSET(s, &m_ReadFDs[SELECT_USE])) {
                if (!np->readCmd()) {
                    XLOG("[%s] connect error %s %p", serverName, np->name, np);
                    np->setNPState(xNetProcessor::NP_DISCONNECT);
                    closeServer(np);
                    continue;
                }

                unsigned char *cmd = 0;
                unsigned short cmdLen;
                bool process_ret = true;
                while (np->getCmd(cmd, cmdLen)) {
                    process_ret = processCmd(np->name, np, cmd, cmdLen);
                    np->popCmd();
                    if (!cmdLen || !process_ret) break;
                }
                if (cmdLen && !process_ret) {
                    xCommand *c = (xCommand *)cmd;
                    if (c) {
                        XERR("[%s] message error %s %p (%u, %u),len:%u", serverName, np->name, np, c->cmd, c->param, cmdLen);
                        //return true;
                    }
                    else {
                        XERR("[%s] message error %s %p", serverName, np->name, np);
                    }

                    np->setNPState(xNetProcessor::NP_DISCONNECT);
                    closeServer(np);
                    continue;
                }
            }
            if (FD_ISSET(s, &m_WriteFDs[SELECT_USE])) {

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

    if (inVerifyList(np)) {
        ServerClient *ta = getConnectedServer(t, n);
        if (ta) {
            if (ta->np) {
                XERR("[%s]%s %s(%p) already in server list:%p", serverName, t, n, np, ta);
                ret = false;
            }
            else {
                if ((_serverInfoList.find(t) == _serverInfoList.end())
                    || (_serverInfoList[t].find(n) == _serverInfoList[t].end())) {
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

        if (_serverList.find(t) != _serverList.end()) {
            if (_serverList[t].find(n) != _serverList[t].end()) {
                _serverList[t][n]->np = np;
                _serverList[t][n]->reset();
                verifyOk(np);
                XLOG("[%s],add to serverList %s", serverName, t);
            }
            else
                XERR("[%s],add to serverList fail %s", serverName, t);
        }
        else
            XERR("[%s],add to serverList fail %s, %s", serverName, t, n);
    }
    //else
    //{
    //    closeServer(np);
    //}
    return ret;
}

bool ZoneServer::runAction() {
    if (checkConnectTimer.elapse(3)) {
        if (checkConnect()) {
            if (!listener)
                startListen(server_port);
            //wwj:与各服务器连接成功以后再初始化一次，临时用，还没有想到更好的方法
            if (!r_init) {
                r_init = true;
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
    if ((t == "") || (_serverList.find(t) == _serverList.end())) return false;
    if ((n != "") && (_serverList[t].find(n) == _serverList[t].end())) return false;

    if (_serverList.find(t) != _serverList.end()) {
        if (n != "") return (_serverList[t].find(n) != _serverList[t].end() && _serverList[t][n]->np);

        for (_server_iter it = _serverList[t].begin(); it != _serverList[t].end(); it++)
            if (NULL == it->second->np) return false;
    }
    else
        return false;

    return true;
}

void ZoneServer::setConnectServerType(std::string t)    //主线程
{
    if ((t == "") || (_serverInfoList.find(t) == _serverInfoList.end()))
        return;

    for (_serverinfo_iter s_it = _serverInfoList[t].begin(); s_it != _serverInfoList[t].end(); s_it++) {
        if (getConnectedServer(t, s_it->first))
            continue;

        _serverList[t][s_it->first] = new ServerClient();

        XDBG("[%s],设置要连接的服务器:%s,%s,%p", serverName, t.c_str(), s_it->first.c_str(), _serverList[t][s_it->first]);
    }
}

//return all connected ok
bool ZoneServer::connectServerByType(std::string t) {
    std::map<std::string, std::map<std::string, serverInfo> > tempList;

    _serverinfo_type_iter type_it;
    _serverinfo_iter serverinfo_it;

    if (t != "") {
        type_it = _serverInfoList.find(t);
        if (type_it == _serverInfoList.end())
            return false;
        tempList.insert(*type_it);
    }
    else
        tempList = _serverInfoList;

    bool ret = true;
    for (type_it = tempList.begin(); type_it != tempList.end(); type_it++) {
        for (_serverinfo_iter s_it = type_it->second.begin(); s_it != type_it->second.end(); s_it++) {
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
                c->addEpoll(server_epfd);
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
    strncpy(send.type, serverType, MAX_NAMESIZE);
    strncpy(send.name, serverName, MAX_NAMESIZE);
    send.zoneID = this->zoneID;
    np->sendCmd(&send, sizeof(send));
}

bool ZoneServer::closeServer(xNetProcessor *np) {

    if (nullptr == np) {
        return false;
    }

    for (_server_type_iter it = _serverList.begin(); it != _serverList.end(); it++) {
        for (_server_iter sub_it = it->second.begin(); sub_it != it->second.end(); sub_it++) {
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

//return first if more than one
ServerClient *ZoneServer::getConnectedServer(std::string t, std::string n) {
    std::map<std::string, std::map<std::string, ServerClient *> > tempList;

    _server_type_iter type_it;
    _server_iter task_it;

    if (t != "") {
        type_it = _serverList.find(t);
        if (type_it == _serverList.end()) return 0;
        tempList.insert(*type_it);
    }
    else
        tempList = _serverList;

    for (type_it = tempList.begin(); type_it != tempList.end(); type_it++) {
        if (n != "")
            task_it = type_it->second.find(n);
        else
            task_it = type_it->second.begin();

        if (task_it == type_it->second.end()) return 0;

        return task_it->second;
    }
    return 0;
}

//return if have
bool ZoneServer::getConnectedServer(std::string t, std::string n, std::vector<ServerClient *> &list) {
    std::map<std::string, std::map<std::string, ServerClient *> > tempList;

    _server_type_iter type_it;
    _server_iter task_it;

    if (t != "") {
        type_it = _serverList.find(t);
        if (type_it == _serverList.end()) return 0;
        tempList.insert(*type_it);
    }
    else
        tempList = _serverList;

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
    std::vector<ServerClient *> list;
    if (!getConnectedServer(type, name != NULL ? name : "", list))
        return false;

    //XDBG("send Cmd ToServer[%s],serverNum[%u]",type,list.size());
    for (UInt32 i = 0; i < list.size(); i++)
        list[i]->sendCmd(data, len);
    return true;
}

bool ZoneServer::sendCmdToFirstServer(void *data, unsigned short len, const char *type) {
    std::vector<ServerClient *> list;
    if (!getConnectedServer(type, "", list))
        return false;

    if (!list.empty())
        list[0]->sendCmd(data, len);
    return true;
}

bool ZoneServer::sendCmdToSession(void *data, unsigned short len) {
    return sendCmdToServer(data, len, "SessionServer");
}

bool ZoneServer::sendCmdToScene(void *data, unsigned short len) {
    return sendCmdToServer(data, len, "SceneServer");
}

bool ZoneServer::sendCmdToRecord(void *data, unsigned short len) {
    return sendCmdToServer(data, len, "RecordServer");
}

bool ZoneServer::sendCmdToLog(void *data, unsigned short len) {
    return sendCmdToServer(data, len, "LogServer");
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
                np->zoneID = rev->zoneID;
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
            if (strncmp(this->serverType, "SceneServer", MAX_NAMESIZE) == 0)
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
    _serverinfo_type_iter type_it = _serverInfoList.find(serverType);
    if (type_it != _serverInfoList.end()) {
        _serverinfo_iter name_it = type_it->second.find(serverName);
        if (name_it != type_it->second.end())
            return name_it->second.ip;
    }
    return NULL;
}

void ZoneServer::setVerifyList(xNetProcessor *task, time_t time) {
    if (!task) return;

    XDBG("[verify_list],Add:%p", task);

    pthread_rwlock_wrlock(&verifylist_critical);
    verify_list[task] = time;
    pthread_rwlock_unlock(&verifylist_critical);
}

void ZoneServer::removeVerifyList(xNetProcessor *task) {
    if (!task) return;

    XDBG("[verify_list],delete:%p", task);

    pthread_rwlock_wrlock(&verifylist_critical);
    verify_list.erase(task);
    pthread_rwlock_unlock(&verifylist_critical);
}

bool ZoneServer::inVerifyList(xNetProcessor *task) {
    if (!task) return false;

    bool retcode = false;

    pthread_rwlock_wrlock(&verifylist_critical);
    retcode = verify_list.find(task) != verify_list.end();
    pthread_rwlock_unlock(&verifylist_critical);

    return retcode;
}

xNetProcessor *ZoneServer::getNpByIPAndPort(UInt32 ip, UInt32 port) {
    pthread_rwlock_wrlock(&verifylist_critical);
    for (verify_iter iter = verify_list.begin(); iter != verify_list.end(); ++iter) {
        if (iter->first && *((UInt32 *)&iter->first->ip) == ip && ((iter->first))->port == port)
            return iter->first;
    }
    pthread_rwlock_unlock(&verifylist_critical);

    return NULL;
}

void ZoneServer::verifyOk(xNetProcessor *task) {
    v_verifyOk(task);
}

bool ZoneServer::getFullName(std::string &dest, std::string src, UInt32 zoneid) {
    dest = src;

    ZoneMap::iterator iter = zoneMap.find(zoneid);
    if (iter == zoneMap.end())
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

