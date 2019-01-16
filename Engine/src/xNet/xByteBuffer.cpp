#include "xByteBuffer.h"
#include "xDefine.h"
#include "xLog.h"


xByteBuffer::xByteBuffer(const char *n, int sockid) {
    //buf.resize(MAX_BUFSIZE*2);
    buf.resize(MAX_BUFSIZE);
    offset = 0;
    name = std::string(n);
    socketID = sockid;
}

xByteBuffer::~xByteBuffer(void) {
}


uint32 xByteBuffer::resize(uint32 len) {
    buf.resize(len);
    return buf.size();
}
/*
 * 数据缓存压栈
 * param 数据
 * param 数据长度
 */
uint32 xByteBuffer::put(const void *data, uint32 len) {
    if (len > MAX_PACKSIZE) {
        XERR("[BufferError],%s,%d,drop packet too big len=%u", name.c_str(), socketID, len);
        return 0;
    }

    while ((buf.size() - offset) < len) {
        resize(buf.size() + MAX_BUFSIZE);
        XTRC("[BufferTrace],%s,%d,buf reserve size=%u offset=%d len=%d", name.c_str(), socketID, buf.size(), offset, len);
    }

    memcpy(getBufOffset(), data, len);
    offset += len;
    return len;
}

uint32 xByteBuffer::put(uint32 len) {
    if ((getUsed() + len) > buf.size()) {
        XERR("[BufferError],%s,%d,read out of buf used=%u len=%u buf=%u", name.c_str(), socketID, getUsed(), len, buf.size());
        return 0;
    }

    offset += len;

    if (getLeft() < MAX_BUFSIZE)
        resize(buf.size() + MAX_BUFSIZE);
    return len;
}

uint32 xByteBuffer::pop(uint32 len) {
    if (len < offset) {
        bcopy(&buf[len], &buf[0], offset - len);
        offset -= len;
    } else if (len > offset) {
        XERR("[BufferError],%s,%d,pop len greater then max len=%u off=%u", name.c_str(), socketID, len, offset);
        offset = 0;;
    } else {
        offset = 0;
    }
    return len;
}
