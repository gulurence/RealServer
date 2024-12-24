#pragma once

#include "xBase/xDefine.h"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/write.hpp>


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




#define MAX_BUFSIZE 65536
#define PH_LEN 4
#define MAX_PACKSIZE (MAX_BUFSIZE-PH_LEN)
#define MIN_PACKSIZE 6//for encrypt

enum PACKET_FLAG_ENUM_TYPE
{
    PACKET_FLAG_COMPRESS = 1,
    PACKET_FLAG_ENCRYPT = 2,
};

struct PacketHead
{
    uint16 flags;
    uint16 len;
    PacketHead() {
        flags = len = 0;
    }
};

struct Packet
{
    PacketHead ph;
    uint8 data[0];

    uint16 getDataSize() { return ph.len; }
    uint16 getFullSize() { return PH_LEN + ph.len; }
};

