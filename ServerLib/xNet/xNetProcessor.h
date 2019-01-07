#pragma once
#include "xSocket.h"
#include "xTime.h"

#ifndef _WINDOWS
#include <arpa/inet.h>
#endif

//网络处理
class xNetProcessor : public xCmdQueue
{
    friend class xServer;
    public:
        xNetProcessor(int fd, const char *n=0);
        virtual ~xNetProcessor();

        enum NPState
        {
            NP_CREATE = 0,
            NP_VERIFIED,
            NP_ESTABLISH,
            NP_DISCONNECT,
            NP_CLOSE,
        };

        void setName(const char *n);
        virtual bool isClient()=0;
        virtual bool isTask()=0;

        void addEpoll(int fd);
        void delEpoll();
        void disconnect();

        bool sendCmd(const void *cmd, unsigned short len);
        bool sendCmd();
        UInt16 getSendLen() const { return m_stSock.getSendLen(); }
        bool readCmd();
        void setComp(bool flag) { m_stSock.setComp(flag); }

        bool getCmd(unsigned char *&cmd, unsigned short &len);
        bool popCmd();

        NPState getNPState() {return m_stNpState;}
        void setNPState(NPState s){m_stNpState = s;}
        const xSocket& GetSock() const {return m_stSock;}

        UInt64 m_u64Id;
        char m_arrcName[MAX_NAMESIZE];

        in_addr m_stIp;
        UInt16 m_u16Port = 0;
        UInt32 m_u32ZoneId = 0;

    protected:
        xSocket m_stSock;
        NPState m_stNpState;
        int m_n32Epfd = -1;
};
