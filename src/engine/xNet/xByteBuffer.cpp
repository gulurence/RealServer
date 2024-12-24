#include "xByteBuffer.h"
#include "xBase/xLog.h"


xByteBuffer::xByteBuffer(tcp_socket stSocket) :m_stSocket(stSocket){
}

xByteBuffer::~xByteBuffer(void) {
}

void xByteBuffer::init() {
    //buf.resize(MAX_NET_CACHE_BUFSIZE*2);
    if (buf.capacity() < MAX_NET_CACHE_BUFSIZE) {
        buf.resize(MAX_NET_CACHE_BUFSIZE);
    }
    offset = 0;
}

uint32 xByteBuffer::resize(uint32 len) {
    buf.resize(len);
    return buf.size();
}
/*
 * 闁轰胶澧楀畵浣虹磽閹惧磭鎽犻柛妯侯儐閻?
 * param 闁轰胶澧楀畵?
 * param 闁轰胶澧楀畵渚€姊归崹顔碱唺
 */
uint32 xByteBuffer::put(const void *data, uint32 len) {
    if (len > MAX_NET_CACHE_BUFSIZE)
    {
        XERR("[BufferError],drop packet too big len=%u", len);
        return 0;
    }

    while ((buf.size() - offset) < len)
    {
        resize(buf.size() + MAX_NET_CACHE_BUFSIZE);
        //XTRC("[BufferTrace],%s,%d,buf reserve size=%u offset=%d len=%d", name.c_str(), socketID, buf.size(), offset, len);
    }
    memcpy(getBufOffset(), data, len);
    offset += len;
    return len;
}

uint32 xByteBuffer::put(uint32 len) {
    if ((getUsed() + len) > buf.size())
    {
        //XERR("[BufferError],%s,%d,read out of buf used=%u len=%u buf=%u", name.c_str(), socketID, getUsed(), len, buf.size());
        return 0;
    }

    offset += len;

    if (getLeft() < MAX_NET_CACHE_BUFSIZE)
        resize(buf.size() + MAX_NET_CACHE_BUFSIZE);
    return len;
}

uint32 xByteBuffer::pop(uint32 len) {
    if (len < offset)
    {
        bcopy(&buf[len], &buf[0], offset - len);
        offset -= len;
    }
    else if (len > offset)
    {
        //XERR("[BufferError],%s,%d,pop len greater then max len=%u off=%u", name.c_str(), socketID, len, offset);
        offset = 0;;
    }
    else
    {
        offset = 0;
    }
    return len;
}







