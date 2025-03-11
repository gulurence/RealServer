#pragma once

#include "xDefine.h"
#include "xObjPool.h"



// 用于 Record 传递Actor数据
struct RecordDataST : public xObjPoolBase
{
public:
    RecordDataST(const int32& i32Len, const char* pData) {
        m_i32DataLen = i32Len;
        m_i32Capacity = i32Len;
        m_pData = new char[m_i32DataLen];
        memcpy(m_pData, pData, m_i32DataLen);
    }
    RecordDataST(const RecordDataST& stData) {
        if (this == &stData) {
            return;
        }

        if (m_i32Capacity < stData.m_i32DataLen) {
            if (m_pData) {
                delete[]m_pData;
                m_pData = nullptr;
            }

            m_i32Capacity = stData.m_i32DataLen;
            m_pData = new char[m_i32DataLen];
        }

        m_i32DataLen = stData.m_i32DataLen;
        memcpy(m_pData, stData.m_pData, m_i32DataLen);
    }
    RecordDataST(RecordDataST&& stData) noexcept {
        if (this == &stData) {
            return;
        }

        int32 i32Type = m_i32Type;
        int64 i64Id = m_i64Id;
        int32 i32DataLen = m_i32DataLen;
        char* pData = m_pData;
        int32 i32Capacity = m_i32Capacity;

        m_i32Type = stData.m_i32Type;
        m_i64Id = stData.m_i64Id;
        m_i32DataLen = stData.m_i32DataLen;
        m_pData = stData.m_pData;
        m_i32Capacity = stData.m_i32Capacity;

        stData.m_i32Type = i32Type;
        stData.m_i64Id = i64Id;
        stData.m_i32DataLen = i32DataLen;
        stData.m_pData = pData;
        stData.m_i32Capacity = i32Capacity;
    }
    ~RecordDataST() {
        if (m_pData) {
            delete[]m_pData;
            m_pData = nullptr;
        }
        m_i32Type = 0;
        m_i64Id = 0;
        m_i32DataLen = 0;
        m_i32Capacity = 0;
    }
    void Resize(int32 i32Size) {
        if (i32Size <= m_i32Capacity) {
            m_i32DataLen = 0;
            return;
        }

        if (m_pData) {
            delete[]m_pData;
            m_pData = nullptr;
        }
        m_i32DataLen = 0;
        m_i32Capacity = i32Size;
        m_pData = new char[m_i32Capacity];
    }

public:
    int32 m_i32Type = 0;
    int64 m_i64Id = 0;

private:
    int32 m_i32DataLen = 0;
    char* m_pData = nullptr;
    int32 m_i32Capacity = 0;

public:
    virtual void cleanUp() {
        m_i32Type = 0;
        m_i64Id = 0;
        m_i32DataLen = 0;
    }

public:
    int32& MutableSize() {
        return m_i32DataLen;
    }
    char* MutableData() {
        return m_pData;
    }
    bool SetSize(const int32 i32Size) {
        if (i32Size > m_i32Capacity) {
            return false;
        }
        m_i32DataLen = i32Size;
        return true;
    }

public:
    int32 Size() const {
        return m_i32DataLen;
    }
    int32 Capacity() const {
        return m_i32Capacity;
    }
    char* Data() const {
        return m_pData;
    }
};

