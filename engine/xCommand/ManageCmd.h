#pragma once
#include "xBase/xDefine.h"

#pragma pack(1)

namespace Cmd
{
    struct ManageCmd : public xCommand
    {
        ManageCmd(uint8 p) : xCommand(MANAGE_CMD, p) {
        }
    };
    
}

#pragma pack()







