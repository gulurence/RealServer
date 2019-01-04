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
           bool onReadComplete(UInt32 len);
           bool onWriteComplete(UInt32 len);
           */

        bool getCmd(UInt8 *&cmd, UInt16 &len);
        bool popCmd();

        bool sendCmd(const void *data, UInt16 len);
        bool sendCmd();
        UInt16 getSendLen() const { return sendLen; }

        bool readToBuf();
        //void bufToCmd();

    protected:
        UInt16 sizeMod8(UInt16 len);//resize by 8 bytes, for encrypt

        bool writeToBuf(void *data, UInt32 len);

        UInt32 compress(void *data, UInt32 len);
        UInt32 uncompress(void *dest, UInt32 destLen, void *src, UInt32 srcLen);

        void encrypt(void *data, UInt16 len);
        void decrypt(void *data, UInt16 len);
        bool needEnc(){return encFlag;}
        bool needDec(){return encFlag;}

        int fd;

        xByteBuffer writeBuffer;
        xByteBuffer readBuffer;
        xByteBuffer encBuffer;

    private:

        bool encFlag;    //加密、解密标志
        bool compFlag;    //压缩标志
        UInt16 sendLen;    //发送的消息长度

        UInt32 unCompCmdRealSize;//未解压的消息长度
        xByteBuffer tmpWriteBuffer;    //临时存储压缩、加密数据
        xByteBuffer tmpDecBuffer;    //存放解密后的数据
        xByteBuffer cmdBuffer;        //存放解压后的数据

        //xCmdBuffer cmdQueue;
};
