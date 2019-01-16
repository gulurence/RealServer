#pragma once

#include "xDefine.h"
#include "xPoolBase.h"
#include "xNetProcessor.h"
#include "xSingletonTemplate.h"

// ���ؽڵ�״̬
enum eBalanceNodeState
{
    eBalanceNodeState_Disconnect = 0,
    eBalanceNodeState_Running,
    eBalanceNodeState_Full,
    eBalanceNodeState_Error,
};


// ���ؽڵ�����
struct BalanceNodeST final
    : public PoolBase<BalanceNodeST, 256>
{
    eBalanceNodeState eState = eBalanceNodeState_Disconnect;
    string strServerType;
    string strServerName;
    uint32 u32CurRunCount = 0;
    uint32 u32MaxRunCount = 0;
    // xNetProcessor *
    void *pNet = 0;

    BalanceNodeST &operator=(const BalanceNodeST& in) {
        if (this == &in) {
            return *this;
        }

        eState = in.eState;
        strServerType = in.strServerType;
        strServerName = in.strServerName;
        u32CurRunCount = in.u32CurRunCount;
        u32MaxRunCount = in.u32MaxRunCount;
        pNet = in.pNet;

        return *this;
    }
};


/* *****************************************************
 *
 *  
 *
 *
 * *****************************************************/

// servername,���ؽڵ���Ϣ
typedef map<string, BalanceNodeST*> BalanceNodeMap;
// servertype,�����б�
typedef map<string, BalanceNodeMap> BalanceMap;

class CBalanceMgr final
{
public:
    CBalanceMgr();
    virtual ~CBalanceMgr();
       
public:
    // ��Ӹ��ؽڵ�
    bool addBalanceNode(BalanceNodeST *);
    bool removeBalanceNode(void *pNetProcess);

private:
    // �����������ͷ������ĸ��ط������б�
    BalanceMap m_mapBalance;


public:

};


///  <���嵥��>
#ifdef _WINDOWS

template ChaSingleton<CBalanceMgr>;
typedef ChaSingleton<CBalanceMgr> BalanceMgrInst;

#else

#define BalanceMgrInst ChaSingleton<CBalanceMgr>

#endif
