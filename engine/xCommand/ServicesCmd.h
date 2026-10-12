#pragma once

#include "xCommand.h"

#pragma pack(1)

#define SERVICESCMD 2
struct ServicesCmd : public xCommand
{
    ServicesCmd(uint8 p) : xCommand(SERVICES_CMD, p)
    {
    }
};

#pragma pack()
