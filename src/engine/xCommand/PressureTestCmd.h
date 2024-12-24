#pragma once

#include "xCommand.h"


#pragma pack(1)

//old change
#define PRESSURE_TEST_CMD 250
struct PressureTestCmd : public xCommand
{
    PressureTestCmd(unsigned char p=0):xCommand(PRESSURE_TEST_CMD, p){}//xCommand(SYSTEM_CMD, p){}
};


#define BASE_TEST_SYSCMD 1
struct BasePressureTestSystemCmd : public PressureTestCmd
{
    BasePressureTestSystemCmd():PressureTestCmd(BASE_TEST_SYSCMD){
    }

    int32 srcServerTypeId = -1;
    int32 targetServerTypeId = -1;
    uint32 len = 0;
    char arrData[512] = {0};
};

#pragma pack()







