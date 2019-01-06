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
        UInt16 getSendLen() const { return sock.getSendLen(); }
        bool readCmd();
        void setComp(bool flag) { sock.setComp(flag); }

        bool getCmd(unsigned char *&cmd, unsigned short &len);
        bool popCmd();

        NPState getNPState() {return np_state;}
        void setNPState(NPState s){np_state = s;}
        const xSocket& GetSock() const {return sock;}

        UInt64 id;
        char name[MAX_NAMESIZE];

        in_addr ip;
        UInt16 port;
        UInt32 zoneID;

    protected:
        xSocket sock;
        NPState np_state;
        int epfd;
};
