#pragma once

#include "xNetDefine.h"

struct xCmd
{
    unsigned short u16Len;
    unsigned char pszData[0];
};

class xCmdQueue
{
public:
    xCmdQueue() {
        //__gnu_cxx::__pool_base::_Tune t_opt(4, 5120, 32, 5120, 10, 10, false);
        //__mt_alloc._M_set_options(t_opt);
#ifdef _LX_DEBUG
        XLOG("[xCmdQueue],闁告帗绻傞～鎰板礌?%p", this);
#endif
    }
    virtual ~xCmdQueue() {
        Clean();

#ifdef _LX_DEBUG
        XLOG("[xCmdQueue],闁哄鍔栭悗?%p", this);
#endif
    }
    void Put(unsigned char* pCmd, unsigned short u16Len) {
        if (!pCmd) return;
        //unsigned char *ch = __mt_alloc.allocate(MAX_BUFSIZE);
#ifndef _WIN32
        unsigned char* ch = __mt_alloc.allocate(len + sizeof(xCmd));
#else
        unsigned char* ch = new unsigned char[u16Len + sizeof(xCmd)];
#endif

        ((xCmd*)ch)->u16Len = u16Len;
        bcopy(pCmd, ((xCmd*)ch)->pszData, (uint32)u16Len);
        bool b = false;
        {
            std::lock_guard<std::mutex> lock(m_critical);
            m_queueCmd.push(std::make_pair(b, (xCmd*)ch));
            m_queueCmd.back().first = true;
        }
    }
    void PutEx(unsigned char* pCmd, unsigned short u16Len) {
        if (!pCmd) return;
        ((xCmd*)pCmd)->u16Len = u16Len;
        bool b = false;
        {
            std::lock_guard<std::mutex> lock(m_critical);
            m_queueCmd.push(std::make_pair(b, (xCmd*)pCmd));
            m_queueCmd.back().first = true;
        }
    }
    bool Get(unsigned char*& cmd, unsigned short& len) {
        cmd = 0;
        len = 0;

        std::lock_guard<std::mutex> lock(m_critical);
        if (!m_queueCmd.empty()) {
            if (m_queueCmd.front().first) {
                xCmd* p = m_queueCmd.front().second;
                if (p) {
                    len = p->u16Len;
                    cmd = p->pszData;
                }
                return true;
            }
        }
        return false;
    }
    bool Pop() {
        std::lock_guard<std::mutex> lock(m_critical);
        if (!m_queueCmd.empty()) {
            xCmd* p = m_queueCmd.front().second;
            m_queueCmd.pop();
            if (p) {    //__mt_alloc.deallocate((unsigned char *)p, MAX_BUFSIZE);
#ifndef _WIN32
                __mt_alloc.deallocate((unsigned char*)p, p->len + sizeof(xCmd));
#else
                delete[]p;
                p = NULL;
#endif
            }
            return true;
        }
        return false;
    }
    void Clean() {
        std::lock_guard<std::mutex> lock(m_critical);
        while (!m_queueCmd.empty()) {
            xCmd* p = m_queueCmd.front().second;
            m_queueCmd.pop();
            if (p) {    //__mt_alloc.deallocate((unsigned char *)p, MAX_BUFSIZE);
#ifndef _WIN32
                __mt_alloc.deallocate((unsigned char*)p, p->len + sizeof(xCmd));
#else
                delete[]p;
                p = NULL;
#endif
            }
        }
    }

    //char* GetRecivePtr() {
    //    return &m_pszCurReciveMsg[m_i32Offset];
    //}
    //void AddReciveSize(int i32Size) {
    //    m_i32Offset += i32Size;
    //}
    //int GetReciveMaxSize() {
    //    return m_i32Capacity - m_i32Offset;
    //}
    //void PushMsg() {

    //    PutEx(m_pszCurReciveMsg, m_i32Offset);
    //    m_i32Offset = 0;
    //    m_pszCurReciveMsg = new unsigned char[m_i32Capacity];
    //}

protected:
    //uint32 readPos, writePos;
    typedef std::pair<bool, xCmd*> CmdPair;
    std::queue<CmdPair> m_queueCmd;

#ifndef _WIN32
    __gnu_cxx::__mt_alloc<unsigned char> __mt_alloc;
#endif

    std::mutex m_critical;

private:
    // 当前接受位置
    int m_i32Offset = 0;
    // 总容量
    int m_i32Capacity = 0;
    // 当前正在读取的协议
    unsigned char *m_pszCurReciveMsg = nullptr;
};
