/********************************************************************
            Copyright (c) 2009, XXXXXX鐠侊紕鐣婚張铏诡潠閹垛偓閼测€插敜閺堝妾洪崗顒€寰?
                   All rights reserved

    閸掓稑缂撻弮銉︽埂閿?2009楠?2閺?0閺?10閺?5閸?
    閺傚洣娆㈤崥宥囆為敍?SimpleAllocator.h
    鐠?   閺勫函绱?缁犫偓閸楁洜娈戦崘鍛摠閸掑棝鍘ら崳?
                濞? 鐎圭偟骞囬崣鍌濃偓鍐х啊Tom Gambill, NCsoft閻ㄥ嫯顔曠拋鈽呯礉楠炶泛婀崗璺虹唨绾偓娑撳﹨绻樼悰?
                    娴滃棔绱崠鏍ф嫲娣囶喗顒?

    瑜版挸澧犻悧鍫熸拱閿?1.00
    娴?   閼板拑绱?
    濮?   鏉╁府绱?閸掓稑缂?

    瑜版挸澧犻悧鍫熸拱閿?1.01
    娴?   閼板拑绱?
    濮?   鏉╁府绱?婢х偛濮炴径姘卞殠缁嬪鍞寸€涙ê鍨庨柊宥呮珤閿涙瓉TSimpleAllocator
*********************************************************************/
#pragma once

#include <vector>
#include "xSynlock.h"
using namespace std;

template <
    typename T,              // 閸愬懎鐡ㄩ崚鍡涘帳閻ㄥ嫮琚崹?
    size_t blocksPerBatch    // 濮ｅ繑顐煎鈧潏鐔奉樋鐏忔垳閲淭閸愬懎鐡ㄧ粚娲？
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

    // 閻㈠疇顕稉鈧稉鐚呮径褍鐨惃鍕敶鐎涙鈹栭梻?
    T* allocateBlock();
    // 闁插﹥鏂?
    void releaseBlock(T* pBlock);

private:
    // 閻劋绨€涙ê鍋嶉悽瀹狀嚞閸掓壆娈慣缁屾椽妫?
    TVector objectVector;
    // 閻劋绨€涙ê鍋嶉悽瀹狀嚞閸掓壆娈戠粚娲？閸︽澘娼冮敍灞肩┒娴滃酣鍣撮弨?
    ByteVector batchVector;
    // 鐠佹澘缍嶆稉瀣╃娑撶寘閻㈠疇顕惃鍕秴缂?
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
    typename T,              // 閸愬懎鐡ㄩ崚鍡涘帳閻ㄥ嫮琚崹?
    size_t blocksPerBatch    // 濮ｅ繑顐煎鈧潏鐔奉樋鐏忔垳閲淭閸愬懎鐡ㄧ粚娲？
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

    // 閻㈠疇顕稉鈧稉鐚呮径褍鐨惃鍕敶鐎涙鈹栭梻?
    T* allocateBlock();
    // 闁插﹥鏂?
    void releaseBlock(T* pBlock);

private:
    // 閻劋绨€涙ê鍋嶉悽瀹狀嚞閸掓壆娈慣缁屾椽妫?
    TVector objectVector;
    // 閻劋绨€涙ê鍋嶉悽瀹狀嚞閸掓壆娈戠粚娲？閸︽澘娼冮敍灞肩┒娴滃酣鍣撮弨?
    ByteVector batchVector;
    // 鐠佹澘缍嶆稉瀣╃娑撶寘閻㈠疇顕惃鍕秴缂?
    size_t nextAllocation;
    // 闁?
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



