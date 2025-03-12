#pragma once

#include "../xBase/xDefine.h"

#pragma pack(1)
struct DatabaseTableCol{
}__attribute__((packed));
#pragma pack()


struct DBConfigST
{
    std::string title;
    std::string host;
    int port = 0;
    std::string name;
    std::string user;
    std::string passwd;
    int poolCount = 0;

    const DBConfigST& operator=(const DBConfigST& in) {
        if (this == &in) {
            return *this;
        }

        title = in.title;
        host = in.host;
        port = in.port;
        name = in.name;
        user = in.user;
        passwd = in.passwd;
        poolCount = in.poolCount;

        return *this;
    }
};


