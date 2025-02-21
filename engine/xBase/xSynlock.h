/********************************************************************
            Copyright (c) 2011, XXXXXX鐠侊紕鐣婚張铏诡潠閹垛偓閼测€插敜閺堝妾洪崗顒€寰?
                   All rights reserved
         
    閸掓稑缂撻弮銉︽埂閿?2011楠?閺?6閺?10閺?1閸?
    閺傚洣娆㈤崥宥囆為敍?SynLock.h
    鐠?   閺勫函绱?缁捐法鈻奸崥灞绢劄鐎电钖?    
    
    瑜版挸澧犻悧鍫熸拱閿?1.00
    娴?   閼板拑绱?
    濮?   鏉╁府绱?   

*********************************************************************/
#pragma once

#include "xDefine.h"

#if defined __LINUX__
#include <pthread.h>

#if !defined __CYGWIN__
#include <pthread.h>
#endif

#endif

namespace SynLock
{
    class ILockObj
    {
    public:
        virtual void Lock    ()    = 0;
        virtual void Unlock    ()    = 0;
    };

    class CCriticalSection : public ILockObj
    {
    public:
        CCriticalSection() {
#if defined WIN32
            InitializeCriticalSection(&m_CritSec);
#elif defined __LINUX__
            pthread_mutex_init(&mutex_lock, nullptr);
#endif
        };

        virtual ~CCriticalSection() {
#if defined WIN32
            DeleteCriticalSection(&m_CritSec);
#elif defined __LINUX__
            pthread_mutex_destroy(&mutex_lock);
#endif
        };

        void Lock(void) {
#if defined WIN32
            EnterCriticalSection(&m_CritSec);
#elif defined __LINUX__
            pthread_mutex_lock(&mutex_lock);
#endif
        };

        void Unlock(void) {
#if defined WIN32
        LeaveCriticalSection(&m_CritSec);
#elif defined __LINUX__
        pthread_mutex_unlock(&mutex_lock);
#endif
        };

    private:
#if defined WIN32
        CRITICAL_SECTION m_CritSec;
#elif defined __LINUX__
        pthread_mutex_t mutex_lock;
#endif
    };

#if defined WIN32
    class CMutexLock : public ILockObj
    {
    public:
        CMutexLock() : m_hMutex(nullptr) {}
        ~CMutexLock(void)    { if (m_hMutex) ::CloseHandle(m_hMutex); }
        
        void Lock    (void)    { if (m_hMutex) ::WaitForSingleObject(m_hMutex, INFINITE); }
        void Unlock    (void)    { if (m_hMutex) ::ReleaseMutex(m_hMutex); }
        
    private:
        HANDLE m_hMutex;
        
    public:
        static CMutexLock* CreateNew(const char* pszName = nullptr, bool bExistTest = false) {
            CMutexLock* pMutex = new CMutexLock;
            
            pMutex->m_hMutex = ::CreateMutexA(nullptr, false, pszName);
            if (!pMutex->m_hMutex) {
                delete pMutex;
                return nullptr;
            }
            
            if (pszName && bExistTest) {
                if (ERROR_ALREADY_EXISTS == ::GetLastError()) {
                    delete pMutex;
                    return nullptr;
                }
            }
            
            return pMutex;
        }
    };
#elif defined __LINUX__ && !defined __CYGWIN__
    class CMutexLock : public ILockObj
    {
    public:
        CMutexLock() {pthread_mutex_init(&m_hMutex, nullptr);}
        ~CMutexLock(void)    {pthread_mutex_destroy(&m_hMutex);}

        void Lock    (void)    {pthread_mutex_lock(&m_hMutex);}
        void Unlock    (void)    {pthread_mutex_unlock(&m_hMutex);}

    private:
        pthread_mutex_t m_hMutex;

    public:
        static CMutexLock* CreateNew(const char* pszName = nullptr, bool bExistTest = false) {
            return new CMutexLock;
        }
    };
#endif 

    class CSingleLock 
    {
    public:
        CSingleLock(ILockObj* pLock) : m_pLock(pLock) {         
            if (nullptr != m_pLock) {
                m_pLock->Lock(); 
            }
        }

        ~CSingleLock() {
            if (nullptr != m_pLock) {
                m_pLock->Unlock();
            }
        }

    private:
        ILockObj* m_pLock;
    };
} 







