/********************************************************************
            Copyright (c) 2009, XXXXXX
                   All rights reserved

    文件名: PoolBase.h
    功能: 内存池基类，提供高效的块式对象分配

    版本: 1.00
    创建: 2009-12-10

    描述:
        PoolBase 使用块式内存分配策略来管理对象的创建和销毁。
        避免频繁的 new/delete 操作，提高内存分配效率。
        模板参数: T (对象类型), BatchSize (每块对象数量), Alloc (分配器策略)

        使用示例:
            class SubClass : public PoolBase<SubClass, 100>
            {
            ......
            }

            ...
            // 
            SubClass *pSubClass = new SubClass();
            ...
            // 
            delete pSubClass;

        注意: PoolBase 不支持数组分配（如 new T[12]），
              因为数组分配使用的是全局 operator new[] 而非池分配器。
              如需数组，请使用 std::vector 或手动管理。

    版本: 1.01
    更新: 增加 MTPoolBase 线程安全变体
*********************************************************************/
#pragma once

#include <vector>
#include "xSimpleAllocator.h"

#ifndef AllowPoolBase
template <
    typename T,             // 池中存储的对象类型
    size_t BatchSize        // 每次分配的对象数量（块大小）
>
struct PoolBase
{
};

// 默认实现为空（需要定义 AllowPoolBase 宏启用）
template <typename T>
struct MTPoolBase
{
};

#else

template <
    typename T,                                               // 池中存储的对象类型
    size_t BatchSize,                                         // 每次分配的对象数量（块大小）
    template <typename, size_t> class Alloc = SimpleAllocator // 分配器策略（默认 SimpleAllocator）
>
class PoolBase
{
    typedef Alloc<T, BatchSize> MyAlloc;
public:
    virtual ~PoolBase() {
    }

    // 重载 new 操作符（从池中分配）
    void * operator new(size_t s) {
        return myAlloc.allocateBlock();
    }

    void operator delete(void *p) {
        myAlloc.releaseBlock((T *)p);
    }

private:
    // 禁用数组分配（数组不使用池分配器）
    void* operator new[](size_t s) {
        return nullptr;
    }

        void operator delete[](void* p) {
    };

private:
    // 静态分配器实例（所有对象共享）
    static MyAlloc myAlloc;
};

template <typename T, size_t BatchSize, template <typename, size_t> class Alloc>
typename PoolBase<T, BatchSize, Alloc>::MyAlloc PoolBase<T, BatchSize, Alloc>::myAlloc;


template <
    typename T,                                               // 池中存储的对象类型
    size_t BatchSize,                                         // 每次分配的对象数量（块大小）
    template <typename, size_t> class Alloc = MTSimpleAllocator // 分配器策略（默认 MTSimpleAllocator，线程安全）
>
class MTPoolBase
{
    typedef Alloc<T, BatchSize> MTMyAlloc;
public:
    virtual ~MTPoolBase() {
    }

    // 重载 new 操作符（从池中分配，线程安全）
    void* operator new(size_t s) {
        return myAlloc.allocateBlock();
    }

    void operator delete(void* p) {
        myAlloc.releaseBlock((T*)p);
    }

private:
    // 禁用数组分配
    void* operator new[](size_t s) {
        return nullptr;
    }

        void operator delete[](void* p) {
    };

private:
    // 静态分配器实例（所有对象共享，线程安全）
    static MTMyAlloc myAlloc;
};

template <typename T, size_t BatchSize, template <typename, size_t> class Alloc>
typename MTPoolBase<T, BatchSize, Alloc>::MTMyAlloc MTPoolBase<T, BatchSize, Alloc>::myAlloc;

#endif // AllowPoolBase







