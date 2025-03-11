#pragma once

#include "xDefine.h"

class xObjPoolBase
{
public:
    xObjPoolBase() {}
    virtual ~xObjPoolBase() {}

private:
    uint32 m_u32PoolIndex____ = 0;

public:
    virtual void cleanUp(){}

public:
    uint32 GetPoolID___() {
        return m_u32PoolIndex____;
    }
    void SetPoolID___(uint32 poolIndex) {
        m_u32PoolIndex____ = poolIndex;
    }
};

template<class T>
class xObjPool
{
public:
    xObjPool(void) {
    }

    ~xObjPool(void) {
        Term();
        //Assert(m_papObj == nullptr);
    }

public:
    bool bInit = false;
    // 娣囨繂鐡ㄧ粚娲＝閻ㄥ嫯濡悙褰掓Щ濮濄垹顦垮▎鈥崇秺鏉?
    std::map<T*, int> mFreeMap;

public:
    bool Init(int32 nMaxCount) {
        //Assert(nMaxCount > 0);
        if (nMaxCount <= 0) {
            return false;
        }

        m_nMaxCount = nMaxCount;
        m_nPosition = 0;
        m_papObj = new T*[m_nMaxCount];

        for (int32 i = 0; i < m_nMaxCount; i++)
        {
            m_papObj[i] = new T;
            if (!m_papObj[i])
            {
                //Assert(m_papObj[i] != nullptr);
                return false;
            }

            mFreeMap[m_papObj[i]] = 1;
        }

        bInit = true;
        return true;
    }

    void Term(void) {
        if (m_papObj != nullptr)
        {
            for (int32 i = 0; i < m_nMaxCount; i++)
            {
                if (m_papObj[i]) {
                    delete m_papObj[i];
                    m_papObj[i] = nullptr;
                }
            }

            delete[] m_papObj;
            m_papObj = nullptr;
        }

        m_nMaxCount = -1;
        m_nPosition = -1;

        mFreeMap.clear();
    }

    T* NewObj(void) {
        std::lock_guard<std::mutex> guard(m_Lock);
        //Assert(m_nPosition < m_nMaxCount);
        if (m_nPosition >= m_nMaxCount)
        {
            Resize(m_nMaxCount + 1024);
            //m_Lock.unlock();
            //return nullptr;
        }

        T *pObj = m_papObj[m_nPosition];
        pObj->SetPoolID___((uint32)m_nPosition);
        m_nPosition++;

        mFreeMap.erase(pObj);
        return pObj;
    }

    bool Resize(int32 nMaxCount) {
        //Assert(nMaxCount > 0);
        if (nMaxCount <= m_nMaxCount) {
            return false;
        }

        int32 i32PreMaxCount = m_nMaxCount;
        m_nMaxCount = nMaxCount;
        T **m_papObjT = new T*[m_nMaxCount];

        // 閺冄呮畱閺佺増宓佺拋鍓х枂鏉╁洦娼? 
        for (int32 i = 0; i < i32PreMaxCount; i++) {
            m_papObjT[i] = m_papObj[i];
        }
        // 閺傛壆鏁电拠椋庢畱閺佷即鍣? 
        for (int32 i = i32PreMaxCount; i < m_nMaxCount; i++) {
            m_papObjT[i] = new T;
            if (m_papObjT[i] == nullptr) {
                //Assert(m_papObj[i] != nullptr);
                return false;
            }
            mFreeMap[m_papObjT[i]] = 1;
        }
        delete[]m_papObj;
        m_papObj = m_papObjT;
        return true;
    }

    void DeleteObj(T *pObj) {

        std::lock_guard<std::mutex> guard(m_Lock);
        //Assert(pObj != nullptr);
        if (pObj == nullptr) {
            return;
        }
        // 閼哄倻鍋ｅ鑼病閸︺劎鈹栭梻鎻掑灙鐞涖劋鑵戞禍?
        if (mFreeMap.find(pObj)!= mFreeMap.end()) {
            return;
        }
        //Assert(m_nPosition > 0);
        if (m_nPosition <= 0) {
            return;
        }

        uint32 uDelIndex = pObj->GetPoolID___();
        //Assert(uDelIndex < (uint32)m_nPosition);
        if (uDelIndex >= (uint32)m_nPosition) {
            return;
        }

        //------------------------------------------------------------//
        //|   ...   |  ...      |        uDelIndex     |     ...       |      m_nPosition-1  |     m_nPosition (瀵板懎鍨庨柊宥嗙潨 ) |

        //|   ...   |  ...      |      m_nPosition -1 | ...            |   m_nPosition(瀵板懘顥ｉ柊宥嗙潨) |

        m_nPosition--;
        T *pDelObj = m_papObj[uDelIndex];
        m_papObj[uDelIndex] = m_papObj[m_nPosition];
        m_papObj[m_nPosition] = pDelObj;

        m_papObj[uDelIndex]->SetPoolID___(uDelIndex);
        m_papObj[m_nPosition]->SetPoolID___(-1);

        mFreeMap[pObj] = 1;

        pObj->cleanUp();
        return;
    }

    int32 GetCount(void)const {
        return m_nPosition;
    }

private:
    T                **m_papObj = nullptr;
    int32            m_nMaxCount = 0;
    int32            m_nPosition = 0;
    std::mutex        m_Lock;
};



