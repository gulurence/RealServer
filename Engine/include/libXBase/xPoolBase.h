/********************************************************************
            Copyright (c) 2009, XXXXXX������Ƽ��ɷ����޹�˾
                   All rights reserved

    �������ڣ� 2009��12��10�� 10ʱ44��
    �ļ����ƣ� PoolBase.h
    ˵    ���� �ڴ�ػ�����ڴ�ػ���ʵ��

    ��ǰ�汾�� 1.00
    ��    �ߣ� 
    ��    ���� ����
    ʹ��˵��:   ������һ�������࣬����ʵ����������װ�˳ص�ʵ�ֺͶ�T��new��delete������������.
                �κ���ػ�����һ������(�̳�)�����࣬�;��гصĹ��ܡ�������һ��ģ���࣬������
                ģ��������ֱ���: T, BatchSize, Alloc�����ǹ��ܶ����ǣ�
                1. T
                   Ҫ�ػ�������
                2. BatchSize
                   ÿ������صĴ�С�������ٶ��ٸ�T�ڴ�ռ�
                3. Alloc
                   �ط�����, ��һ���Ĺ��ʵ�֣��û�Ҳ���Լ�ʵ�ֹ�񻯵ķ�����

               ʹ������:
                   class SubClass : public PoolBase<SubClass, 100>
                   {
                   ......
                   }

                   ...
                   // �ӳ��������ڴ��
                   SubClass *pSubClass = new SubClass();
                   ...
                   // ���ڴ�鷵������
                   delete pSubClass;


               ps: ���಻֧�ֶ���������룬�� new T[12]���������±���ʧ�ܡ�������������ĵ���
                   PoolBase ��֧�ֶ��̡߳�
               ����
    ��ǰ�汾�� 1.01
    ��    �ߣ� 
    ��    ���� ���Ӷ��߳�ģ����MTPoolBase, ʹ�÷���ͬPoolBase��
*********************************************************************/
#pragma once
#include <vector>
#include "xSimpleAllocator.h"

#ifndef AllowPoolBase
template <
    typename T,             // �ڴ���������
    size_t BatchSize        // ÿ�ο��ٶ��ٸ�T�ڴ�ռ�
>
struct PoolBase
{
};

// ���߳�
template <typename T>
struct MTPoolBase
{
};

#else

template <
    typename T,                                               // �ڴ���������
    size_t BatchSize,                                         // ÿ�ο��ٶ��ٸ�T�ڴ�ռ�
    template <typename, size_t> class Alloc = SimpleAllocator // ��������Ĭ��ΪSimpleAllocator
>
class PoolBase
{
    typedef Alloc<T, BatchSize> MyAlloc;
public:
    virtual ~PoolBase() {
    }

    // ����T�� new �� delete
    void * operator new(size_t s) {
        return myAlloc.allocateBlock();
    }

    void operator delete(void *p) {
        myAlloc.releaseBlock((T *)p);
    }

private:
    // ��֧����������
    void* operator new[](size_t s) {
        return nullptr;
    }

        void operator delete[](void* p) {
    };

private:
    // ���T��������ʵ���ĵ�һ�ط���
    static MyAlloc myAlloc;
};

template <typename T, size_t BatchSize, template <typename, size_t> class Alloc>
typename PoolBase<T, BatchSize, Alloc>::MyAlloc PoolBase<T, BatchSize, Alloc>::myAlloc;


template <
    typename T,                                               // �ڴ���������
    size_t BatchSize,                                         // ÿ�ο��ٶ��ٸ�T�ڴ�ռ�
    template <typename, size_t> class Alloc = MTSimpleAllocator // ��������Ĭ��ΪMTSimpleAllocator
>
class MTPoolBase
{
    typedef Alloc<T, BatchSize> MTMyAlloc;
public:
    virtual ~MTPoolBase() {
    }

    // ����T�� new �� delete
    void* operator new(size_t s) {
        return myAlloc.allocateBlock();
    }

    void operator delete(void* p) {
        myAlloc.releaseBlock((T*)p);
    }

private:
    // ��֧����������
    void* operator new[](size_t s) {
        return nullptr;
    }

        void operator delete[](void* p) {
    };

private:
    // ���T��������ʵ���ĵ�һ�ط���
    static MTMyAlloc myAlloc;
};

template <typename T, size_t BatchSize, template <typename, size_t> class Alloc>
typename MTPoolBase<T, BatchSize, Alloc>::MTMyAlloc MTPoolBase<T, BatchSize, Alloc>::myAlloc;

#endif // AllowPoolBase
