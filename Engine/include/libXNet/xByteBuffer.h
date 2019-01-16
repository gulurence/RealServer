#pragma once
#include "xDefine.h"
#include "xTools.h"
#include "xNetDefine.h"


#ifndef _WINDOWS
#include <ext/mt_allocator.h>
#endif
class xNetProcessor;


class xByteBuffer
{
public:
    xByteBuffer(const char *n, int sockid);
    ~xByteBuffer();

    uint32 size() {
        return buf.size();
    }
    uint32 resize(uint32 len);

    uint32 put(const void *data, uint32 len);//used when write
    uint32 put(uint32 len);//used when read complete
    uint32 pop(uint32 len);//used when write complete

    uint32 getUsed() {
        return offset;
    }
    uint32 getLeft() {
        return buf.size() - offset;
    }
    void *getBufOffset() {
        return &buf[offset];
    }
    void *getBufOffset(uint32 off) {
        return &buf[off];
    }
    void *getBufBegin() {
        return &buf[0];
    }
    void reset() {
        offset = 0;
    }

protected:
    std::vector<unsigned char> buf;
    uint32 offset;
private:
    std::string name;
    int socketID;
};

struct xCmd
{
    unsigned short len;
    unsigned char data[0];
};
//}__attribute__((packed));

class xCmdQueue
{
public:
    xCmdQueue() {
#ifdef _LX_DEBUG
        XLOG("[xCmdQueue],初始化,%p", this);
#endif
    }
    virtual ~xCmdQueue() {
        unique_lock<shared_mutex> wl(m_mutex);

        while (!cmdQueue.empty()) {
            pop();
        }
#ifdef _LX_DEBUG
        XLOG("[xCmdQueue],析构,%p", this);
#endif
    }
    void put(unsigned char *cmd, unsigned short len) {
        if (!cmd) return;
        //unsigned char *ch = __mt_alloc.allocate(MAX_BUFSIZE);
#ifndef _WINDOWS
        unsigned char *ch = __mt_alloc.allocate(len + sizeof(xCmd));
#else
        unsigned char *ch = new unsigned char[len + sizeof(xCmd)];
#endif

        ((xCmd *)ch)->len = len;
        bcopy(cmd, ((xCmd *)ch)->data, (uint32)len);
        bool b = false;

        // 写锁 
        unique_lock<shared_mutex> wl(m_mutex);

        cmdQueue.push(std::make_pair(b, (xCmd *)ch));
        cmdQueue.back().first = true;
    }

    bool get(unsigned char *&cmd, unsigned short &len) {
        cmd = 0;
        len = 0;

        // 读锁
        unique_lock<shared_mutex> wl(m_mutex);

        if (!cmdQueue.empty()) {
            if (cmdQueue.front().first) {
                xCmd *p = cmdQueue.front().second;
                if (p) {
                    len = p->len;
                    cmd = p->data;
                }

                return true;
            }
        }

        return false;
    }

    bool pop() {

        // 读锁
        unique_lock<shared_mutex> wl(m_mutex);

        if (!cmdQueue.empty()) {
            xCmd *p = cmdQueue.front().second;
            cmdQueue.pop();
            if (p) {    //__mt_alloc.deallocate((unsigned char *)p, MAX_BUFSIZE);
#ifndef _WINDOWS
                __mt_alloc.deallocate((unsigned char *)p, p->len + sizeof(xCmd));
#else
                delete[]p;
                p = NULL;
#endif
            }

            return true;
        }

        return false;
    }
protected:
    //uint32 readPos, writePos;
    typedef std::pair<bool, xCmd *> CmdPair;
    std::queue<CmdPair> cmdQueue;

#ifndef _WINDOWS
    __gnu_cxx::__mt_alloc<unsigned char> __mt_alloc;
#endif

    shared_mutex m_mutex;
};
