/********************************************************************
            Copyright (c) 2009, XXXXXX
                   All rights reserved

    文件名: SimpleAllocator.h
    功能: 简单内存分配器，基于块式策略进行内存管理
          参考: Tom Gambill, NCsoft 的内存池实现

    版本: 1.00
    创建: 2009-12-05

    版本: 1.01
    更新: 增加 MTSimpleAllocator 线程安全变体
*********************************************************************/
#pragma once

#include <vector>
#include "xSynlock.h"
using namespace std;

template <
    typename T,              // 
    size_t blocksPerBatch    // 
>
class SimpleAllocator
{
public:
    typedef vector<T*> TVector;
    typedef vector<unsigned char*> ByteVector;

public:
    SimpleAllocator() : nextAllocation(0) {
    }

    ~SimpleAllocator();

    // 分配一个新的对象块
    T* allocateBlock();
    // 释放对象块
    void releaseBlock(T* pBlock);

private:
    // 对象向量：存储所有分配的对象
    TVector objectVector;
    // 批次向量：存储已分配的批次
    ByteVector batchVector;
    // 下一个可用对象索引
    size_t nextAllocation;
};

template <typename T, size_t blocksPerBatch>
SimpleAllocator<T, blocksPerBatch>::~SimpleAllocator() {
    //return;
    size_t iNum = batchVector.size();
    for (size_t i = 0; i < iNum; ++i) {
        unsigned char* p = batchVector[i];
        delete[]p;
    }
}

template <typename T, size_t blocksPerBatch>
void SimpleAllocator<T, blocksPerBatch>::releaseBlock(T* pBlock) {
    if (pBlock) {
        objectVector[--nextAllocation] = pBlock;
    }
}

template <typename T, size_t blocksPerBatch>
T* SimpleAllocator<T, blocksPerBatch>::allocateBlock() {
    if (nextAllocation >= objectVector.size()) {
        size_t st_bpb = 0 == blocksPerBatch ? 1 : blocksPerBatch;
        size_t size = sizeof(T) * st_bpb;

        unsigned char* pBatch = new unsigned char[size];
        batchVector.push_back(pBatch);

        size = objectVector.size() + st_bpb;
        objectVector.resize(size);

        size_t iNew = nextAllocation;
        for (size_t i = 0; i < st_bpb; ++i) {
            objectVector[iNew++] = (T*)&(pBatch[sizeof(T) * i]);
        }
    }

    return objectVector[nextAllocation++];
}

template <
    typename T,              // 分配的对象类型
    size_t blocksPerBatch    // 每次分配的块数量
>
class MTSimpleAllocator
{
public:
    typedef vector<T*> TVector;
    typedef vector<unsigned char*> ByteVector;

public:
    MTSimpleAllocator() : nextAllocation(0) {
    }

    ~MTSimpleAllocator();

    // 分配一个新的对象块（线程安全）
    T* allocateBlock();
    // 释放对象块（线程安全）
    void releaseBlock(T* pBlock);

private:
    // 对象向量：存储所有分配的对象
    TVector objectVector;
    // 批次向量：存储已分配的批次
    ByteVector batchVector;
    // 下一个可用对象索引
    size_t nextAllocation;
    // 临界区锁（保证线程安全）
    SynLock::CCriticalSection cslocker;
};

template <typename T, size_t blocksPerBatch>
MTSimpleAllocator<T, blocksPerBatch>::~MTSimpleAllocator() {
    SynLock::CSingleLock locker(&cslocker);
    //return;
    size_t iNum = batchVector.size();
    for (size_t i = 0; i < iNum; ++i) {
        unsigned char* p = batchVector[i];
        delete[]p;
    }
}

template <typename T, size_t blocksPerBatch>
void MTSimpleAllocator<T, blocksPerBatch>::releaseBlock(T* pBlock) {
    SynLock::CSingleLock locker(&cslocker);

    if (pBlock) {
        objectVector[--nextAllocation] = pBlock;
    }
}

template <typename T, size_t blocksPerBatch>
T* MTSimpleAllocator<T, blocksPerBatch>::allocateBlock() {
    SynLock::CSingleLock locker(&cslocker);

    if (nextAllocation >= objectVector.size()) {
        size_t st_bpb = 0 == blocksPerBatch ? 1 : blocksPerBatch;
        size_t size = sizeof(T) * st_bpb;

        unsigned char* pBatch = new unsigned char[size];
        batchVector.push_back(pBatch);

        size = objectVector.size() + st_bpb;
        objectVector.resize(size);

        size_t iNew = nextAllocation;
        for (size_t i = 0; i < st_bpb; ++i) {
            objectVector[iNew++] = (T*)&(pBatch[sizeof(T) * i]);
        }
    }

    return objectVector[nextAllocation++];
}



