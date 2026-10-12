#pragma once

#include "xBase/xDefine.h"
#include "xBase/xThread.h"
#include "xLog/xLog.h"

#include <asio.hpp>
#include <functional>
#include <memory>

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
#pragma pack()

// =============================================================================
// 客户端协议帧 (Phase 5): [uint32 u32Len][protobuf body]
// body 前两个字段为 BigType(一级 Actor 类型) / SmallType(二级子模块) 用于路由。
// =============================================================================
#pragma pack(1)
struct ClientFrameHeadST
{
    uint32 u32Len = 0; // body 长度 (protobuf 序列化字节数)
};
#pragma pack()
#define ClientFrameHeadSize sizeof(ClientFrameHeadST)

struct NetPackageST
{
    NetPackageST(uint32 size = 128)
    {
        u32Size = size;
        if (pPackageData)
        {
            delete[] pPackageData;
            pPackageData = nullptr;
        }
        pPackageData = new uint8[u32Size];
    }
    ~NetPackageST()
    {
        if (pPackageData)
        {
            delete[] pPackageData;
            pPackageData = nullptr;
        }
    }
    void ReSize(uint32 size)
    {
        if (size <= u32Size)
        {
            return;
        }
        if (pPackageData)
        {
            delete[] pPackageData;
            pPackageData = nullptr;
        }
        if (size > u32Size * 2)
        {
            u32Size = size;
        }
        else
        {
            u32Size = u32Size * 2;
        }
        pPackageData = new uint8[u32Size];
    }

public:
    uint32 u32Size = 0;
    uint8 *pPackageData = nullptr;
};

using asio::as_tuple_t;
using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::use_awaitable_t;
using asio::ip::tcp;
using default_token = as_tuple_t<use_awaitable_t<>>;
using tcp_acceptor = default_token::as_default_on_t<tcp::acceptor>;
using tcp_socket = default_token::as_default_on_t<tcp::socket>;
namespace this_coro = asio::this_coro;

#define NetPackageHeadSize sizeof(NetPackageHeadST)

typedef std::shared_ptr<NetPackageST> NetPackageSharedPtr;
typedef NetPackageHeadST *NetPackageHeadSharedPtr;

// 客户端链接ID
typedef uint64 NetCID;

// 返回上层给出的唯一ID用于后续的发送消息
typedef std::function<NetCID(void *, tcp_socket &)> NetPackageAcceptPtr;
typedef std::function<void(void *, tcp_socket &, uint64, NetPackageSharedPtr)> NetPackageCallPtr;
typedef std::function<void(void *, NetCID)> NetDisconnectPtr;
// 连接建立/断开时注册 socket (供发送通道 SendToCID 使用; nullptr 表示断开)
typedef std::function<void(void *, NetCID, std::shared_ptr<tcp_socket>)> NetSocketReadyPtr;

// xNetAgent 配置结构
struct NetAgentConfigST
{
    NetAgentConfigST()
    {
    }
    NetAgentConfigST(const NetAgentConfigST &stConfig)
    {
        if (this == &stConfig)
        {
            return;
        }
        strServiceName = stConfig.strServiceName;
        strHost = stConfig.strHost;
        u16Port = stConfig.u16Port;
        pAcceptCall = stConfig.pAcceptCall;
        pPackageCall = stConfig.pPackageCall;
        pDisconnect = stConfig.pDisconnect;
        pSocketReady = stConfig.pSocketReady;
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
    NetSocketReadyPtr pSocketReady;

public:
    uint16 u16CacheBuffSize = 1024;

public:
    void *pParam = nullptr;
};
