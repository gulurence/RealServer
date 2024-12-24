#pragma once

#include "xNetDefine.h"
#include "xByteBuffer.h"

#ifndef _WIN32
#include <sys/epoll.h>
#endif

class xSocket
{
public:
    xSocket(tcp_socket& stSocket);
    ~xSocket();

    void init();
    void cleanup();

    tcp_socket &get_fd() const { return m_stSocket; }

    bool valid() { /*return fd != -1;*/ return true; }
    void shutdown(int how);
    void close();

    bool setNonBlock();
    bool setSockOpt();
    void setComp(bool flag) { compFlag = flag; }
    
    bool getCmd(uint8 *&cmd, uint16 &len);
    bool popCmd();

    bool sendCmd(const void *data, uint16 len);
    bool sendCmd();
    uint16 getSendLen() const { return sendLen; }

    bool readToBuf();
    //void bufToCmd();

protected:
    uint16 sizeMod8(uint16 len);//resize by 8 bytes, for encrypt

    bool writeToBuf(void *data, uint32 len);

    uint16 compress(void *data, uint64 len);
    uint16 uncompress(void *dest, uint64 destLen, void *src, uint64 srcLen);

    void encrypt(void *data, uint16 len);
    void decrypt(void *data, uint16 len);
    bool needEnc() { return encFlag; }
    bool needDec() { return encFlag; }


//public:
//    int fd = -1;
//    xByteBuffer writeBuffer;
//    xByteBuffer readBuffer;
//    xByteBuffer encBuffer;

private:

    bool encFlag;
    bool compFlag;
    uint16 sendLen;

    uint32 unCompCmdRealSize;

//public:
//    xByteBuffer tmpWriteBuffer;
//    xByteBuffer tmpDecBuffer;
//    xByteBuffer cmdBuffer;

private:
    tcp_socket& m_stSocket;
};







