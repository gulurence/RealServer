#pragma once
#include "xDefine.h"
#include "xTools.h"
#include "xNetDefine.h"
//#include "xMemory.h"
#ifndef _WINDOWS
#include <ext/mt_allocator.h>
#else
#include "pthreads.2/pthread.h"
#endif
#include <vector>
#include <queue>
class xNetProcessor;

/*
class xCmdBuffer
{
public:
    xCmdBuffer(){}
    ~xCmdBuffer()
    {
        while (!cmdQueue.empty())
        {
            xMemory::put(cmdQueue.front().second);
            cmdQueue.pop();
        }
    }
    void putCmd(Packet *p)
    {
        if (!p) return;
        //unsigned char *ch = new unsigned char[p->getFullSize()];
        unsigned char *ch = xMemory::get(p->getFullSize());
        bcopy(p, ch, (UInt32)p->getFullSize());
        bool b = false;
        cmdQueue.push(std::make_pair(b, (Packet *)ch));
        cmdQueue.back().first = true;
    }
    bool getCmd(unsigned char *&cmd, unsigned short &len)
    {
        cmd = 0;
        len = 0;
        if (cmdQueue.empty()) return false;

        if (cmdQueue.front().first)
        {
            Packet *p = cmdQueue.front().second;
            len = p->ph.len;
            cmd = p->data;
            cmdQueue.pop();
            return true;
        }
        return false;
    }
    bool popCmd()
    {
        if (cmdQueue.empty()) return false;

        unsigned char *p = (unsigned char *)cmdQueue.front().second;
        cmdQueue.pop();
        //SAFE_DELETE_VEC(p);
        xMemory::put(p);
        return true;
    }
protected:
    //UInt32 readPos, writePos;
    typedef std::pair<bool, Packet *> CmdPair;
    std::queue<CmdPair> cmdQueue;
};*/

class xByteBuffer
{
    public:
        xByteBuffer(const char *n,int sockid);
        ~xByteBuffer();

        UInt32 size(){return buf.size();}
        UInt32 resize(UInt32 len);

        UInt32 put(const void *data, UInt32 len);//used when write
        UInt32 put(UInt32 len);//used when read complete
        UInt32 pop(UInt32 len);//used when write complete

        UInt32 getUsed(){return offset;}
        UInt32 getLeft(){return buf.size()-offset;}
        void *getBufOffset(){return &buf[offset];}
        void *getBufOffset(UInt32 off){return &buf[off];}
        void *getBufBegin(){return &buf[0];}
        void reset(){offset = 0;}

    protected:
        std::vector<unsigned char> buf;
        UInt32 offset;
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
        xCmdQueue()
        {
            int ret = pthread_rwlock_init(&m_critical, 0);
            if (0!=ret)
                XERR("初始化读写锁失败 ret=%d", ret);
            //__gnu_cxx::__pool_base::_Tune t_opt(4, 5120, 32, 5120, 10, 10, false);
            //__mt_alloc._M_set_options(t_opt);
#ifdef _LX_DEBUG
            XLOG("[xCmdQueue],初始化,%p", this);
#endif
        }
        virtual ~xCmdQueue()
        {
            pthread_rwlock_wrlock(&m_critical);
            while (!cmdQueue.empty())
            {
                pop();
            }
            pthread_rwlock_unlock(&m_critical);
            pthread_rwlock_destroy(&m_critical);
#ifdef _LX_DEBUG
            XLOG("[xCmdQueue],析构,%p", this);
#endif
        }
        void put(unsigned char *cmd, unsigned short len)
        {
            if (!cmd) return;
            //unsigned char *ch = __mt_alloc.allocate(MAX_BUFSIZE);
#ifndef _WINDOWS
            unsigned char *ch = __mt_alloc.allocate(len+sizeof(xCmd));
#else
            unsigned char *ch = new unsigned char[len+sizeof(xCmd)];
#endif

            ((xCmd *)ch)->len = len;
            bcopy(cmd, ((xCmd *)ch)->data, (UInt32)len);
            bool b = false;
            pthread_rwlock_wrlock(&m_critical);
            cmdQueue.push(std::make_pair(b, (xCmd *)ch));
            cmdQueue.back().first = true;
            pthread_rwlock_unlock(&m_critical);
        }
        bool get(unsigned char *&cmd, unsigned short &len)
        {
            cmd = 0;
            len = 0;
            pthread_rwlock_wrlock(&m_critical);
            if (!cmdQueue.empty())
            {
                if (cmdQueue.front().first)
                {
                    xCmd *p = cmdQueue.front().second;
                    if (p)
                    {
                        len = p->len;
                        cmd = p->data;
                    }
                    pthread_rwlock_unlock(&m_critical);
                    return true;
                }
            }
            pthread_rwlock_unlock(&m_critical);
            return false;
        }
        bool pop()
        {
            pthread_rwlock_wrlock(&m_critical);
            if (!cmdQueue.empty())
            {
                xCmd *p = cmdQueue.front().second;
                cmdQueue.pop();
                if (p)
                {    //__mt_alloc.deallocate((unsigned char *)p, MAX_BUFSIZE);
#ifndef _WINDOWS
                    __mt_alloc.deallocate((unsigned char *)p, p->len+sizeof(xCmd));
#else
                    delete []p;
                    p = NULL;
#endif
                }
                pthread_rwlock_unlock(&m_critical);
                return true;
            }
            pthread_rwlock_unlock(&m_critical);
            return false;
        }
    protected:
        //UInt32 readPos, writePos;
        typedef std::pair<bool, xCmd *> CmdPair;
        std::queue<CmdPair> cmdQueue;

#ifndef _WINDOWS
        __gnu_cxx::__mt_alloc<unsigned char> __mt_alloc;
#endif

        pthread_rwlock_t m_critical;
};
