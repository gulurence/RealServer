#pragma once

#include "xBase/xDefine.h"
#include "xBase/xThread.h"
#include "xLog/xLog.h"

#include <boost/asio.hpp>
#include <functional>


#define IP_SIZE 16

#pragma pack(1)
struct NetPackageHeadST
{
    uint16 u16HeadPrefix = 0;
    uint32 u32MsgId = 0;
    uint64 u64SrcActorId = 0;
    uint64 u64TargetActorId = 0;
    uint16 u16BodySize = 0;
};
struct NetPackageST
{
    NetPackageST(uint16 size=128) {
        u16Size = size;
        if (pPackageData) {
            delete[]pPackageData;
            pPackageData = nullptr;
        }
        pPackageData = new uint8[u16Size];
    }
    void ReSize(uint16 size) {
        if (size <= u16Size) {
            return;
        }
        if (pPackageData) {
            delete[]pPackageData;
            pPackageData = nullptr;
        }
        if (size > u16Size *2) {
            u16Size = size;
        } else {
            u16Size = u16Size * 2;
        }
        pPackageData = new uint8[u16Size];
    }
public:
    uint16 u16Size = 0;
    uint8* pPackageData = nullptr;
};
#pragma pack()

using boost::asio::as_tuple_t;
using boost::asio::ip::tcp;
using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable_t;
using default_token = as_tuple_t<use_awaitable_t<>>;
using tcp_acceptor = default_token::as_default_on_t<tcp::acceptor>;
using tcp_socket = default_token::as_default_on_t<tcp::socket>;
namespace this_coro = boost::asio::this_coro;



#define NetPackageHeadSize sizeof(NetPackageHeadST)

typedef std::shared_ptr<NetPackageST> NetPackageSharedPtr;
typedef NetPackageHeadST* NetPackageHeadSharedPtr;

// 客户端链接ID
typedef uint64 NetCID;


// 返回上层给出的唯一ID用于后续的发送消息
typedef std::function<NetCID(void *,tcp_socket&)> NetPackageAcceptPtr;
typedef std::function<void(void*, tcp_socket&, uint64, NetPackageSharedPtr)> NetPackageCallPtr;
typedef std::function<void(void*, NetCID)> NetDisconnectPtr;




// xNetAgent 配置结构
struct NetAgentConfigST {
    NetAgentConfigST(){}
    NetAgentConfigST(const NetAgentConfigST &stConfig) {
        if (this==&stConfig) {
            return;
        }
        strServiceName = stConfig.strServiceName;
        strHost = stConfig.strHost;
        u16Port = stConfig.u16Port;
        pAcceptCall = stConfig.pAcceptCall;
        pPackageCall = stConfig.pPackageCall;
        pDisconnect = stConfig.pDisconnect;
        u16CacheBuffSize = stConfig.u16CacheBuffSize;
    }

public:
    std::string strServiceName;

public:
    std::string strHost;
    uint16 u16Port = 0;

public:
    NetPackageAcceptPtr pAcceptCall;
    NetPackageCallPtr pPackageCall;
    NetDisconnectPtr pDisconnect;

public:
    uint16 u16CacheBuffSize = 1024;

public:
    void* pParam = nullptr;
};

