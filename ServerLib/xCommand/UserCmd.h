#pragma once
#include "xCommand.h"
#include "UserData.h"

#pragma pack(1)

#define LOGIN_CMD 1 //登录
#define SCENE_USER_CMD 2 //场景数据
#define DATA_USER_CMD 3		//user数据指令
#define SYSTEM_USER_CMD 4	//系统指令
#define TRADE_USER_CMD 5 	//摆摊，npc买卖，邮件，拍卖等相关指令
#define TEAM_USER_CMD 6		//组队指令
#define EQUIP_USER_CMD 7	//装备
#define SEPT_USER_CMD 8		//家族
#define CITY_USER_CMD 9		//城市
#define FRIEND_USER_CMD 10	//好友系统
#define COUNTRY_USER_CMD 11	//国家
#define PET_USER_CMD 13	//宠物

namespace Cmd
{
	/*
	* 用户命令大分支
	*/
	struct UserCmd : public xCommand
	{
		UserCmd(UInt8 c, UInt8 p) : xCommand(c, p)
		{
		}
	};
}

#define MAX_NOTICE_SIZE 60

#pragma pack()
