#pragma once

#include "xDefine.h"
#include "xPoolBase.h"
#include "xNetProcessor.h"
#include "xSingletonTemplate.h"

// 负载节点状态
enum eBalanceNodeState
{
    eBalanceNodeState_Disconnect = 0,
    eBalanceNodeState_Running,
    eBalanceNodeState_Full,
    eBalanceNodeState_Error,
};


// 负载节点数据
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

// servername,负载节点信息
typedef map<string, BalanceNodeST*> BalanceNodeMap;
// servertype,负载列表
typedef map<string, BalanceNodeMap> BalanceMap;

class CBalanceMgr final
{
public:
    CBalanceMgr();
    virtual ~CBalanceMgr();
       
public:
    // 添加负载节点
    bool addBalanceNode(BalanceNodeST *);
    bool removeBalanceNode(void *pNetProcess);

private:
    // 管理所有类型服务器的负载服务器列表
    BalanceMap m_mapBalance;


public:

};


///  <定义单例>
#ifdef _WINDOWS

template ChaSingleton<CBalanceMgr>;
typedef ChaSingleton<CBalanceMgr> BalanceMgrInst;

#else

#define BalanceMgrInst ChaSingleton<CBalanceMgr>

#endif
