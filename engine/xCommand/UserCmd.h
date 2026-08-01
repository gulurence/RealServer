#pragma once
#include "xCommand.h"
//#include "UserData.h"

#pragma pack(1)

#define LOGIN_CMD 1 //
#define SCENE_USER_CMD 2 //
#define DATA_USER_CMD 3        //
#define SYSTEM_USER_CMD 4    //
#define TRADE_USER_CMD 5     //
#define TEAM_USER_CMD 6        //
#define EQUIP_USER_CMD 7    //
#define SEPT_USER_CMD 8        //
#define CITY_USER_CMD 9        //
#define FRIEND_USER_CMD 10    //
#define COUNTRY_USER_CMD 11    //
#define PET_USER_CMD 13    //

namespace Cmd
{
    /*
    * 闁活潿鍔嶉崺娑㈠川閹存帗濮㈠鍫嗗啫鐎婚柡鈧?
    */
    struct UserCmd : public xCommand
    {
        UserCmd(uint8 c, uint8 p) : xCommand(c, p)
        {
        }
    };
}

#define MAX_NOTICE_SIZE 60

#pragma pack()



