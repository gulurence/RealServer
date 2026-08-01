#pragma once

#include "tThridServiceConfig.h"
#include <atomic>

// =============================================================================
// ThridService — 所有第三方库服务的基类
//
// 公共字段: type, id, name, create_time
// 生命周期: Init(config) → Release()
// 引用计数: AddRef() / DecRef() / GetRefCount()
//
// 子类: tServiceMySql, tServiceRedis, tServiceLog, tServiceXml, tServicePlatformMgr
// 统一管理: tServiceMgr (singleton)
// =============================================================================
class ThridService
{
public:
    ThridService(EThridServiceType enType, int32 id, const std::string& name);
    virtual ~ThridService();

    // --- 公共数据字段 ---
    EThridServiceType GetType() const       { return m_enType; }
    int32             GetID() const         { return m_i32ID; }
    const std::string& GetName() const      { return m_strName; }
    int64             GetCreateTime() const { return m_i64CreateTime; }

    // --- 生命周期 (子类实现) ---
    // 使用 ThridServiceConfig 中对应自身类型的配置字段
    virtual bool Init(const ThridServiceConfig& cfg) = 0;
    virtual void Release() = 0;
    bool IsInitialized() const { return m_bInitialized; }

    // --- 引用计数 ---
    int32 AddRef()    { return ++m_i32RefCount; }
    int32 DecRef()    { return --m_i32RefCount; }
    int32 GetRefCount() const { return m_i32RefCount.load(); }

protected:
    EThridServiceType m_enType;
    int32       m_i32ID = 0;
    std::string m_strName;
    int64       m_i64CreateTime = 0;

    std::atomic<int32> m_i32RefCount{0};
    bool m_bInitialized = false;
};
