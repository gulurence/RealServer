#pragma once

#include "xBase/xDefine.h"
#include "xNet/ServerClient.h"

typedef std::map<std::string, ServerClient*> SConnectMap;


class ZoneServer;
// 绠＄悊鏈嶅姟鍣ㄩ棿鐨勯摼鎺?
// 缃戠粶灞傜殑澶勭悊 鏁版嵁璇诲彇鍙戦€佺瓑
class xSConnectManager
{
public:
    xSConnectManager(ZoneServer* pServer);
    ~xSConnectManager() {}

private:
    ZoneServer* m_pZoneServer = nullptr;

public:
#ifndef _WIN32
    int server_epfd = 0;//epfd 
    struct epoll_event server_ev[MAX_SERVER_EVENT];

public:
    void addEpoll(int fd);

public:
    int getEpollFD() {
        return server_epfd;
    }

#else
    SOCKET            m_MinFD = 0;
    SOCKET            m_MaxFD = 0;
    INT               m_nFDSize = 0;

public:
    //缃戠粶鏁版嵁  
    enum
    {
        SELECT_BAK = 0,    //  褰撳墠绯荤粺涓嫢鏈夌殑瀹屾暣鍙ユ焺鏁版嵁    
        SELECT_USE = 1,    //  鐢ㄤ簬select璋冪敤鐨勫彞鏌勬暟鎹?  
        SELECT_MAX = 2, //  缁撴瀯浣跨敤鏁伴噺   
    };

public:
    fd_set            m_ReadFDs[SELECT_MAX];
    fd_set            m_WriteFDs[SELECT_MAX];
    fd_set            m_ExceptFDs[SELECT_MAX];
    timeval            m_Timeout[SELECT_MAX];


    std::mutex m_lockClientConnect;
    std::map<xNetProcessor*, int> m_mapClient;

public:
    void            addSocket(xNetProcessor* t);
    void            removeSocket(xNetProcessor* t);

#endif

public:
    // 鎵ц缃戠粶灞傜殑娑堟伅鏀跺彇
    bool processNetwork();
};



