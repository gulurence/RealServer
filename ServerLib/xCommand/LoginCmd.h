#pragma once
#include "UserCmd.h"
#pragma pack(1)

namespace Cmd
{
	/************************* 登录指令 *****************************/
	struct LoginCmd : public UserCmd
	{
		LoginCmd(UInt8 p):UserCmd(LOGIN_CMD, p)
		{
		}
	};

	//client->regserver
#define ACCPASS_REG_USER_CMD 1
	struct AccPassRegUserCmd : public LoginCmd
	{
		AccPassRegUserCmd() : LoginCmd(ACCPASS_REG_USER_CMD)
		{
			bzero(acc, sizeof(acc));
			bzero(pass, sizeof(pass));
			zoneID = 0;
		}
		char acc[MAX_NAMESIZE];
		char pass[MAX_NAMESIZE];
		UInt32 zoneID;
	};

#define SYS_INFO_USER_CMD 2
	struct SysInfoUserCmd : public LoginCmd
	{
		SysInfoUserCmd() : LoginCmd(SYS_INFO_USER_CMD)
		{
			bzero(info, sizeof(info));
		}
		char info[MAX_NAMESIZE];
	};

// regServer -> Client
#define GATE_IP_USER_CMD 3
	struct GateIPUserCmd : public LoginCmd
	{
		GateIPUserCmd() : LoginCmd(GATE_IP_USER_CMD)
		{
			ip = 0;
			port = 0;
			len = 0;
		}
		UInt32 ip;
		int port;
		UInt16 len;
		UInt8 loginData[0];	//登陆数据: sha1|accid|loginTime ...
	};

	struct LoginData
	{
		char sha1[SHA1_LEN];
		UInt64 accid;
		UInt32 loginTime;
		UInt32 zoneid;
		UInt16 addict;
		char source[USER_SOURCE_LEN];
		LoginData() 
		{ 
			bzero(sha1, sizeof(sha1)); 
			accid = 0;
			loginTime = 0;
			zoneid = 0;
			addict = 1;
			bzero(source, sizeof(source)); 
		}
	};

#define REG_GATE_USER_CMD 4
	struct RegGateUserCmd : public LoginCmd
	{
		RegGateUserCmd() : LoginCmd(REG_GATE_USER_CMD)
		{
			loginType = 0;
			len = 0;
		}
		UInt8 loginType;		// 登陆类型 1: 客户端, 2: 网页端
//		char minloginType[MAX_BUFFER_LEN];//登录子分类
		UInt16 len;
		UInt8 loginData[0];	//登陆数据: sha1|accid|loginTime ...
	};

#define REG_ERR_USER_CMD 5
	struct RegErrUserCmd : public LoginCmd
	{
		RegErrUserCmd() : LoginCmd(REG_ERR_USER_CMD)
		{
			bzero(info, sizeof(info));
			ret = ERR_NONE;
		}
		char info[USER_SOURCE_LEN];
		ErrRet ret;
	};

	//GateWay to Client
#define SNAPSHOT_USER_CMD 6
	struct SnapShotUserCmd : public LoginCmd
	{
		UInt8 ret;
		UInt64 createUID;
		SnapShotData data[MAX_ROLE_SIZE];
		SnapShotUserCmd() : LoginCmd(SNAPSHOT_USER_CMD)
		{
			createUID = 0;
			ret = 0; 
		}        
	};

	//client to //Session
#define LOGIN_USER_CMD 7 
	struct LoginUserCmd: public LoginCmd
	{
		UInt64 id;
		LoginUserCmd() : LoginCmd(LOGIN_USER_CMD)
		{
			id = 0;
		}
	};

	//gate to client,login succeed
#define LOGIN_RESULT_USER_CMD 8
	struct LoginResultUserCmd: public LoginCmd
	{
		LoginResultUserCmd() : LoginCmd(LOGIN_RESULT_USER_CMD)
		{
			ret = 0;
		}
		UInt8 ret;	//2可以进入场景
	};

	//创建角色
#define CREATE_CHAR_USER_CMD 9
	struct CreateCharUserCmd: public LoginCmd
	{
		char name[MAX_ROLENAMESIZE]; // 名称
		UInt8 role_career; // 职业
		UInt16 len;
		UInt8 loginData[0];	//登陆数据: sha1|accid|loginTime ...
		CreateCharUserCmd() : LoginCmd(CREATE_CHAR_USER_CMD)
		{
			bzero(name, sizeof(name));
			role_career = 0;
			len = 0;
		}
	};
	
	//删除角色
#define DELETE_CHAR_USER_CMD 10
	struct DeleteCharUserCmd: public LoginCmd
	{
		UInt64 id;
		DeleteCharUserCmd():LoginCmd(DELETE_CHAR_USER_CMD)
		{
			id=0;
		}
	};

	enum
	{
		CREATE_ERROR_NAME_USED 		= 0,	//重名
		CREATE_ERROR_CAN_NOT_USE 	= 1,//含有敏感词汇
		CREATE_ERROR_HAVE_SPACE 	= 2,//含有空格
	};

	//重名
#define NAME_USED_CHAR_USER_CMD 11
	struct NameUsedCharUserCmd: public LoginCmd
	{
		UInt16 type;
		NameUsedCharUserCmd():LoginCmd(NAME_USED_CHAR_USER_CMD)
		{
			type = 0;
		}
	};

	//请求快照
#define REQ_SNAP_CHAR_USER_CMD 12
	struct ReqSnapCharUserCmd: public LoginCmd
	{
		ReqSnapCharUserCmd():LoginCmd(REQ_SNAP_CHAR_USER_CMD)
		{
		}
	};

#define REQ_RANDOMNAME_USER_CMD 13
struct ReqRandomNameUserCmd: public LoginCmd {
	UInt8 sex;
	ReqRandomNameUserCmd() :
		LoginCmd(REQ_RANDOMNAME_USER_CMD) {

		sex=0;
	}
};

#define RET_RANDOWNAME_USER_CMD 14
struct RetRandomNameUserCmd: public LoginCmd {
	UInt8 ret;
	char rolename[MAX_ROLENAMESIZE];

	RetRandomNameUserCmd() :
		LoginCmd(RET_RANDOWNAME_USER_CMD) {
		ret=0;
		bzero(rolename,MAX_ROLENAMESIZE);
	}
};

#define LOADGAMEOK_USER_CMD 15
struct LoadGameOKCmd:public LoginCmd{
	char flagkey[MAX_NAMESIZE];
	LoadGameOKCmd():LoginCmd(LOADGAMEOK_USER_CMD){
			bzero(flagkey,MAX_NAMESIZE);
	}
};
#define LOGIN_GATE_USER_CMD 104
	struct LoginGateUserCmd : public LoginCmd
	{
		UInt64 accid;
		LoginGateUserCmd() : LoginCmd(LOGIN_GATE_USER_CMD)
		{
			accid = 0;
		}
	};

}

#pragma pack()
