#include "zlib/zlib.h"
extern "C" {
#include "des/d3des.h"
}

#include "xSocket.h"
#include <errno.h>
#include <fcntl.h>

//压缩等级: 0 ~ 9, 数字越大，压缩率越高，耗时越长
#define COMPRESS_LEVEL 6
#define MIN_COMPRESS_SIZE 48

UInt8 fixedkey[8] = { 95, 27, 5, 20, 131, 4, 8, 88 };

xSocket::xSocket(int sock) :
        fd(sock), writeBuffer("writeBuffer", sock), readBuffer("readBuffer",
                sock), encBuffer("encBuffer", sock), tmpWriteBuffer(
                "tmpWriteBuffer", sock), tmpDecBuffer("tmpDecBuffer", sock), cmdBuffer(
                "cmdBuffer", sock) {

    writeBuffer.resize(MAX_BUFSIZE);
    readBuffer.resize(MAX_BUFSIZE);
    encBuffer.resize(MAX_BUFSIZE);
    tmpWriteBuffer.resize(MAX_BUFSIZE);
    tmpDecBuffer.resize(MAX_BUFSIZE);
    cmdBuffer.resize(MAX_BUFSIZE);

    unCompCmdRealSize = 0;
    encFlag = false;
    compFlag = false;
    sendLen = 0;

    setSockOpt();
}

xSocket::~xSocket() {
    close();
}

void xSocket::close() {
    if (valid()) {
        SAFE_CLOSE_SOCKET(fd);
        fd = -1;
    }
}

void xSocket::shutdown(int how) {
    if (valid()) {
        XDBG("[Socket]shutdown %d", fd);
        ::shutdown(fd, how);
    }
}

bool xSocket::setNonBlock() {
#ifndef _WINDOWS
    int flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    if (-1 == fcntl(fd, F_SETFL, flags)) {
        XERR("[Socket]setNonBlock failed %d", fd);
        return false;
    }
#else
    ULONG argp = 0;
    if ( ioctlsocket(fd,FIONBIO,&argp) == SOCKET_ERROR ) 
    {
        //INT iErr = WSAGetLastError() ;
        return false ;
    }
#endif
    return true;
}

bool xSocket::setSockOpt() {
    int nRecvBuf = MAX_BUFSIZE * 2;
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (const char*) &nRecvBuf, sizeof(int));
    int nSendBuf = MAX_BUFSIZE * 2;
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (const char*) &nSendBuf, sizeof(int));
    return true;
}

UInt16 xSocket::sizeMod8(UInt16 len) {
    return len;
    return (len + 7) / 8 * 8;
}

bool xSocket::readToBuf() {
    bool final_ret = true;

    while (1) 
    {
        int max = readBuffer.getLeft();
        if (max < MAX_BUFSIZE) {
            readBuffer.resize(readBuffer.size() + MAX_BUFSIZE);
            max = readBuffer.getLeft();
        }

        int ret = ::recv(fd, (char*)readBuffer.getBufOffset(), max, 0);
        if (ret < 0) {
            if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
                final_ret = false;
                XDBG("[SOCKET]1 read errpr %u %u %u",
                        ((Packet *) readBuffer.getBufBegin())->ph.len,
                        ((Packet *) readBuffer.getBufBegin())->data[0],
                        ((Packet *) readBuffer.getBufBegin())->data[1]);
                XERR("[SOCKET] read errpr,errno:%u,%s", errno, strerror(errno));
            }
            //XERR("[SOCKET]接收错误,errno:%u,%s", errno, strerror(errno));
            break;
        } else if (0 == ret) //peer shutdown
                {
            final_ret = false;
            XDBG("[SOCKET]2 read errpr %u %u %u",
                    ((Packet *) readBuffer.getBufBegin())->ph.len,
                    ((Packet *) readBuffer.getBufBegin())->data[0],
                    ((Packet *) readBuffer.getBufBegin())->data[1]);
            XERR("[SOCKET] read errpr,errno:%u,%s", errno, strerror(errno));
            break;
        } else {
            readBuffer.put(ret);
            //XDBG("[SOCKET]收到数据 %u %u %u", ((Packet *)readBuffer.getBufBegin())->ph.len, ((Packet *)readBuffer.getBufBegin())->data[0], ((Packet *)readBuffer.getBufBegin())->data[1]);
            //bufToCmd();
        }

        break;
    }

    return final_ret;
}

bool xSocket::sendCmd() {
    if (!valid())
        return false;
    if (!writeBuffer.getUsed())
        return false;

    bool final_ret = true;

    int all = writeBuffer.getUsed();

    int ret = ::send(fd, (char*)writeBuffer.getBufBegin(), all, 0);
    if (ret < 0) {
        if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
            XERR("[SOCKET]发送错误,fd:%d,errno:%u,%s", fd, errno, strerror(errno));
            final_ret = false;
        }
    } else {
        writeBuffer.pop(ret);
        sendLen = ret;
    }
    return final_ret;
}
/*
 * 发送消息
 * param data 发送的数据
 * param len  数据长度
 */
bool xSocket::sendCmd(const void *data, UInt16 len) {
    if (!valid())
        return false;
    encBuffer.reset();            //重置缓冲区
    PacketHead ph;            //协议头
    encBuffer.put(&ph, PH_LEN);            //封装头4字节
    encBuffer.put(data, len);            //数据长度
    PacketHead *p = (PacketHead *) encBuffer.getBufBegin();
    UInt16 real_size = len;

    if (compFlag/* && len >= MIN_COMPRESS_SIZE*/) {
        real_size = compress(encBuffer.getBufOffset(PH_LEN), len);        //数据压缩
        if (real_size > 0)
            p->flags |= PACKET_FLAG_COMPRESS;            //数据进行压缩选项
        else
            real_size = len;
        //XLOG("[压缩], %u -> %u", len, real_size);
    } else {
        //XLOG("[压缩], %u", len);
    }
    p->len = real_size;
    real_size += PH_LEN;
    if (needEnc()) {
        p->flags |= PACKET_FLAG_ENCRYPT;
        real_size = sizeMod8(real_size);
        encrypt(encBuffer.getBufBegin(), real_size);
    }

    if (!writeToBuf(encBuffer.getBufBegin(), real_size))
        return false;
    return sendCmd();
}

bool xSocket::writeToBuf(void *data, UInt32 len) {
    UInt32 real_size = writeBuffer.put(data, len);

    return real_size == len;
}
/**
 * 压缩数据
 * param data 数据
 * len 数据字节长度
 */
UInt32 xSocket::compress(void *data, UInt32 len) {
    UInt32 newLen = tmpWriteBuffer.size();
    if (Z_OK == ::compress2((Bytef *) tmpWriteBuffer.getBufBegin(),(uLongf *) &newLen, (Bytef *) data, (uLong) len,COMPRESS_LEVEL)) {
        memcpy(data, tmpWriteBuffer.getBufBegin(), newLen);
        return newLen;
    }

    XLOG("[xSocket],fd:%d,压缩错误", fd);
    return 0;
}

/*
 * 解压缩数据
 *
 */
UInt32 xSocket::uncompress(void *dest, UInt32 destLen, void *src, UInt32 srcLen) {
    UInt32 newLen = destLen;
    if (Z_OK == ::uncompress((Bytef *) dest, (uLongf *) &newLen, (Bytef *) src,(uLong) srcLen)) {
        return newLen;
    }

    return 0;
}

void xSocket::encrypt(void *data, UInt16 len) {
    deskey(fixedkey, EN0);
    for (UInt16 i = 0; i < len; i += 8)
        des((UInt8 *) data + i, (UInt8 *) tmpWriteBuffer.getBufOffset(i));
    memcpy(data, tmpWriteBuffer.getBufBegin(), len);
}

void xSocket::decrypt(void *data, UInt16 len) {
    deskey(fixedkey, DE1);
    for (UInt16 i = 0; i < len; i += 8)
        des((UInt8 *) data + i, (UInt8 *) tmpDecBuffer.getBufOffset(i));
    memcpy(data, tmpDecBuffer.getBufBegin(), len);
}

bool xSocket::getCmd(UInt8 *&cmd, UInt16 &len) {
    //return cmdQueue.getCmd(cmd, len);

    cmd = 0;
    len = 0;

    UInt32 used = readBuffer.getUsed();
    if (used < MIN_PACKSIZE)
        return false;
    void * buf = readBuffer.getBufBegin();
    Packet *p = (Packet *) buf;

    UInt16 real_size = 0;

    if (needDec()) {
        UInt8 copy[8];
        memcpy(copy, (UInt8 *) readBuffer.getBufBegin(), 8);
         decrypt(copy, 8);
        real_size = ((Packet *) copy)->getFullSize();
    } else
        real_size = p->getFullSize();

    if (real_size > used) {
        XERR("消息大小不匹配，协议错误:real_size[%u],buffer_size[%u]", real_size, used);
        return false;
    }

    if (needDec())
        decrypt(p, real_size);

    unCompCmdRealSize = real_size;

    if (p->ph.flags & PACKET_FLAG_COMPRESS) {
        len = uncompress(cmdBuffer.getBufBegin(), cmdBuffer.size(), p->data,
                p->ph.len);
        if (len > 0)
            cmd = (UInt8 *) cmdBuffer.getBufBegin();
        else
        XLOG("[xSocket],fd:%d,解压错误,消息长度:%u", fd, p->ph.len);
    } else {
        cmd = p->data;
        len = p->ph.len;
    }

    return true;
}

bool xSocket::popCmd() {
    if (unCompCmdRealSize >= MIN_PACKSIZE
            && unCompCmdRealSize <= readBuffer.getUsed()) {
        if (readBuffer.pop(unCompCmdRealSize)) {
            unCompCmdRealSize = 0;
            return true;
        }
    }

    return false;
}

