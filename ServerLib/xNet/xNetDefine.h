#pragma once
#define MAX_BUFSIZE 65536
#define PH_LEN 4
#define MAX_PACKSIZE (MAX_BUFSIZE-PH_LEN)
#define MIN_PACKSIZE 6//for encrypt

enum PACKET_FLAG_ENUM_TYPE    //包头标志
{
    PACKET_FLAG_COMPRESS = 1,    //压缩
    PACKET_FLAG_ENCRYPT = 2,    //加密
};

struct PacketHead
{
    UInt16 flags;
    UInt16 len;
    PacketHead()
    {
        flags = len = 0;
    }
};

struct Packet
{
    PacketHead ph;
    UInt8 data[0];

    UInt16 getDataSize(){return ph.len;}
    UInt16 getFullSize(){return PH_LEN+ph.len;}
};
