#pragma once
#include "xDefine.h"
#include "xByteBuffer.h"

#ifndef _WINDOWS
#include <sys/epoll.h>
#endif

class xSocket
{
    public:
        xSocket(int sock);
        ~xSocket();

        int get_fd() const {return fd;}

        bool valid(){return fd!=-1;}
        void shutdown(int how);
        void close();

        bool setNonBlock();
        bool setSockOpt();
        void setComp(bool flag) { compFlag = flag; }

        /*
           bool beginRead();
           bool beginWrite();
           bool isReading(){return _isReading;}
           bool isWriting(){return _isWriting;}
           bool onReadComplete(uint32 len);
           bool onWriteComplete(uint32 len);
           */

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

        uint32 compress(void *data, uint32 len);
        uint32 uncompress(void *dest, uint32 destLen, void *src, uint32 srcLen);

        void encrypt(void *data, uint16 len);
        void decrypt(void *data, uint16 len);
        bool needEnc(){return encFlag;}
        bool needDec(){return encFlag;}

        int fd;

        xByteBuffer writeBuffer;
        xByteBuffer readBuffer;
        xByteBuffer encBuffer;

    private:

        bool encFlag;    //加密、解密标志
        bool compFlag;    //压缩标志
        uint16 sendLen;    //发送的消息长度

        uint32 unCompCmdRealSize;//未解压的消息长度
        xByteBuffer tmpWriteBuffer;    //临时存储压缩、加密数据
        xByteBuffer tmpDecBuffer;    //存放解密后的数据
        xByteBuffer cmdBuffer;        //存放解压后的数据

        //xCmdBuffer cmdQueue;
};
