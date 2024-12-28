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

typedef std::function<uint64(tcp_socket&)> NetPackageAcceptPtr;
typedef std::function<void(tcp_socket&, uint64, NetPackageSharedPtr)> NetPackageCallPtr;

