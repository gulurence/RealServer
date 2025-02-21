#pragma once
#include "xCommand.h"
//#include "UserData.h"

#pragma pack(1)

#define LOGIN_CMD 1 //闁谎嗩嚙缂?
#define SCENE_USER_CMD 2 //闁革妇鍎ゅ▍娆撳极閻楀牆绁?
#define DATA_USER_CMD 3        //user闁轰胶澧楀畵渚€骞愰崶锔藉Б
#define SYSTEM_USER_CMD 4    //缂侇垵宕电划娲箰閸ワ附濮?
#define TRADE_USER_CMD 5     //闁硅棄妫欓幉锟犳晬鐎圭棷c濞戞梹婢樺畷鐘绘晬瀹€鍕粡濞寸姾顔愮槐婵嬪箯瀹ュ懎绀岀紒娑橆槺濞村宕楅搹顐㈢樄濞?
#define TEAM_USER_CMD 6        //缂備礁瀚板Σ锕傚箰閸ワ附濮?
#define EQUIP_USER_CMD 7    //閻熶礁鎳庨ˇ?
#define SEPT_USER_CMD 8        //閻庣鍩栧Λ?
#define CITY_USER_CMD 9        //闁糕晛楠哥粩?
#define FRIEND_USER_CMD 10    //濠靛倽妫勫鍝ュ寲閼姐倗鍩?
#define COUNTRY_USER_CMD 11    //闁搞儴妫勯?
#define PET_USER_CMD 13    //閻庡湱濮锋晶?

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



