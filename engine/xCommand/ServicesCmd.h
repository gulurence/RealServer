#pragma once

#include "xCommand.h"

#pragma pack(1)

#define SERVICESCMD 2
struct ServicesCmd: public xCommand {
    ServicesCmd(unsigned char p) :
            xCommand(SERVICES_CMD, p) {

    }
};

#pragma pack()


