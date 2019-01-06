/********************************************************************
            Copyright (c) 2009, XXXXXX������Ƽ��ɷ����޹�˾
                   All rights reserved

    �������ڣ� 2009��12��10�� 10ʱ45��
    �ļ����ƣ� SimpleAllocator.h
    ˵    ���� �򵥵��ڴ������
                ע: ʵ�ֲο���Tom Gambill, NCsoft����ƣ�����������Ͻ���
                    ���Ż�������

    ��ǰ�汾�� 1.00
    ��    �ߣ� shaokuo chen
    ��    ���� ����

    ��ǰ�汾�� 1.01
    ��    �ߣ� shaokuo chen
    ��    ���� ���Ӷ��߳��ڴ��������MTSimpleAllocator
*********************************************************************/
#pragma once
#include <vector>
#include "xSynlock.h"
using namespace std;

template <
    typename T,              // �ڴ���������
    size_t blocksPerBatch    // ÿ�ο��ٶ��ٸ�T�ڴ�ռ�
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

    // ����һ��T��С���ڴ�ռ�
    T* allocateBlock();
    // �ͷ�
    void releaseBlock(T *pBlock);

private:
    // ���ڴ洢���뵽��T�ռ�
    TVector objectVector;
    // ���ڴ洢���뵽�Ŀռ��ַ�������ͷ�
    ByteVector batchVector;
    // ��¼��һ��T�����λ��
    size_t nextAllocation;
};

template <typename T, size_t blocksPerBatch>
SimpleAllocator<T, blocksPerBatch>::~SimpleAllocator() {
    //return;
    size_t iNum = batchVector.size();
    for (size_t i = 0; i < iNum; ++i) {
        unsigned char *p = batchVector[i];
        delete[]p;
    }
}

template <typename T, size_t blocksPerBatch>
void SimpleAllocator<T, blocksPerBatch>::releaseBlock(T *pBlock) {
    if (pBlock) {
        objectVector[--nextAllocation] = pBlock;
    }
}

template <typename T, size_t blocksPerBatch>
T * SimpleAllocator<T, blocksPerBatch>::allocateBlock() {
    if (nextAllocation >= objectVector.size()) {
        size_t st_bpb = 0 == blocksPerBatch ? 1 : blocksPerBatch;
        size_t size = sizeof(T) * st_bpb;

        unsigned char *pBatch = new unsigned char[size];
        batchVector.push_back(pBatch);

        size = objectVector.size() + st_bpb;
        objectVector.resize(size);

        size_t iNew = nextAllocation;
        for (size_t i = 0; i < st_bpb; ++i) {
            objectVector[iNew++] = (T *)&(pBatch[sizeof(T) * i]);
        }
    }

    return objectVector[nextAllocation++];
}

template <
    typename T,              // �ڴ���������
    size_t blocksPerBatch    // ÿ�ο��ٶ��ٸ�T�ڴ�ռ�
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

    // ����һ��T��С���ڴ�ռ�
    T* allocateBlock();
    // �ͷ�
    void releaseBlock(T *pBlock);

private:
    // ���ڴ洢���뵽��T�ռ�
    TVector objectVector;
    // ���ڴ洢���뵽�Ŀռ��ַ�������ͷ�
    ByteVector batchVector;
    // ��¼��һ��T�����λ��
    size_t nextAllocation;
    // ��
    SynLock::CCriticalSection cslocker;
};

template <typename T, size_t blocksPerBatch>
MTSimpleAllocator<T, blocksPerBatch>::~MTSimpleAllocator() {
    SynLock::CSingleLock locker(&cslocker);
    //return;
    size_t iNum = batchVector.size();
    for (size_t i = 0; i < iNum; ++i) {
        unsigned char *p = batchVector[i];
        delete[]p;
    }
}

template <typename T, size_t blocksPerBatch>
void MTSimpleAllocator<T, blocksPerBatch>::releaseBlock(T *pBlock) {
    SynLock::CSingleLock locker(&cslocker);

    if (pBlock) {
        objectVector[--nextAllocation] = pBlock;
    }
}

template <typename T, size_t blocksPerBatch>
T * MTSimpleAllocator<T, blocksPerBatch>::allocateBlock() {
    SynLock::CSingleLock locker(&cslocker);

    if (nextAllocation >= objectVector.size()) {
        size_t st_bpb = 0 == blocksPerBatch ? 1 : blocksPerBatch;
        size_t size = sizeof(T) * st_bpb;

        unsigned char *pBatch = new unsigned char[size];
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
