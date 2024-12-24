#pragma once

#include "../xBase/xTools.h"
#include "../xNet/xNetDefine.h"

class xNetProcessor;

#define MAX_NET_CACHE_BUFSIZE 6553

class xByteBuffer
{
public:
    xByteBuffer(tcp_socket stSocket);
    ~xByteBuffer();

    void init();
    void cleanup() {
        offset = 0;
    }

    uint32 size() { return (uint32)buf.size(); }
    uint32 resize(uint32 len);

    uint32 put(const void* data, uint32 len);//used when write
    uint32 put(uint32 len);//used when read complete
    uint32 pop(uint32 len);//used when write complete

    uint32 getUsed() { return offset; }
    uint32 getLeft() { return buf.size() - offset; }
    void* getBufOffset() { return &buf[offset]; }
    void* getBufOffset(uint32 off) { return &buf[off]; }
    void* getBufBegin() { return &buf[0]; }
    void reset() { offset = 0; }

protected:
    std::vector<unsigned char> buf;
    uint32 offset = 0;

private:
    tcp_socket& m_stSocket;
};
