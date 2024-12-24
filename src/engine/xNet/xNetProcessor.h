#pragma once

#include "xSocket.h"
#include "../xBase/xTime.h"
#include "../xNet/xCmdQueue.h"


class xNetProcessor : public xCmdQueue
{
    friend class xServer;
public:
    xNetProcessor(xSocket *pSocket):m_pSock(pSocket){}
    virtual ~xNetProcessor();

    void init(int fd, const char *n = 0){}
    void cleanup();

    enum NPState
    {
        NP_CREATE = 0,
        NP_VERIFIED,
        NP_ESTABLISH,
        NP_DISCONNECT,
        NP_CLOSE,
    };

    void setName(const char *n);
    virtual bool isClient() { return false; }
    virtual bool isTask() { return false; }

    void addEpoll(int fd);
    void delEpoll();
    void disconnect();

    bool sendCmd(const void *cmd, unsigned short len);
    bool sendCmd();
    uint16 getSendLen() const { return m_pSock->getSendLen(); }
    bool readCmd();
    void setComp(bool flag) { m_pSock->setComp(flag); }

    bool getCmd(unsigned char *&cmd, unsigned short &len);
    bool popCmd();

    NPState getNPState() { return np_state; }
    void setNPState(NPState s) { np_state = s; }
    const xSocket& GetSock() const { return *m_pSock; }
    
private:
    std::mutex m_lockSendMsg;

public:
    uint64 m_u64ID = 0;
    char m_szName[MAX_NAMESIZE] = {0};
    in_addr m_ip;
    uint16 m_port = 0;

public:
    uint32 m_u32SGUID = 0;

public:
    void *m_pUser = nullptr;

protected:
    xSocket *m_pSock;
    NPState np_state;
    int m_epfd = 0;

public:
    uint64 mCCount = 0;
    uint64 mCDataSize = 0;

public:
    void AddCDataSize(uint64 size) {
        mCDataSize += mCDataSize;
    }
};







