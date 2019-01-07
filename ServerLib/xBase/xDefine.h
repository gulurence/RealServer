#ifndef _X_DEFINE_H_
#define _X_DEFINE_H_

#if 1
#pragma warning(disable:4099)//LNK4099
#pragma warning(disable:4996)
#pragma warning(disable:4200)
#pragma warning(disable:4355)
#endif



#ifdef _WINDOWS

typedef char        int8;
typedef short       int16;
typedef int         int32;
typedef long long   int64;

typedef unsigned char       uint8;
typedef unsigned short      uint16;
typedef unsigned int        uint32;
typedef unsigned long long  uint64;

#else

typedef char    int8;
typedef short   int16;
typedef int     int32;
typedef long    int64;

typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned int    uint32;
typedef unsigned long   uint64;

#endif





// =====================================================   以下为windows下面调整的代码============================================ //
#include <stdint.h>
#ifndef _WINDOWS
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <winsock2.h>
#include <string>
#define bzero(p1,p2) memset(p1,0,p2)
#define bcopy(p1,p2,p3) memcpy((void*)p2,(void*)p1,p3)
#define __attribute__(p) ;
#define usleep(t) Sleep(t/1000)
#endif

#ifdef _WINDOWS
#define snprintf _snprintf
#endif


// =====================================================   以下为windows下面调整的代码============================================ //


typedef int8_t        Int8;
typedef int16_t        Int16;
typedef int32_t        Int32;
typedef int64_t        Int64;

typedef uint8_t        UInt8;
typedef uint16_t    UInt16;
typedef uint32_t    UInt32;
typedef uint64_t    UInt64;

typedef UInt64 USER_ID;
const UInt32 UID_LEN=sizeof(USER_ID);
typedef UInt64 NPC_ID;

#define UInt8_MIN ((UInt8)0)
#define UInt8_MAX ((UInt8)0xff)           //255
#define UInt16_MIN ((UInt16)0)
#define UInt16_MAX ((UInt16)0xffff)         //65535
#define UInt32_MIN ((UInt32)0)
#define UInt32_MAX ((UInt32)0xffffffff)   //4294967295
#define UInt64_MIN ((UInt64)0)
#define UInt64_MAX ((UInt64)0xffffffffffffffff)   //18446744073709551615

enum LOAD_CONFIG {
    //加载表格
    LOAD_NPC_BASE = 1,
    LOAD_OBJ_BASE,
    LOAD_EQUIP_BASE,
    LOAD_PET_BASE,
    LOAD_LEVELUP_EXP_TABLE,
    LOAD_SKILL_BASE,
    LOAD_QUEST_LIST,
    LOAD_NPC_BASE_AI,
    LOAD_EQUIP_ZIZHI_BASE,
    LOAD_EXP_BASE,
    LOAD_STAR_UP_BASE,
    LOAD_TABLE_ALL=999,
    //加载xml配置
    LOAD_AI_CONFIG = 1000,
    LOAD_ATTR_BASE_VALUE,
    LOAD_LEVELUP_POINT_CONFIG,
    LOAD_MASSIVE_CONFIG,
    LOAD_DROP_CONFIG,
    LOAD_ITEM_CONFIG,
    LOAD_EQUIP_BASE_CONFIG,
    LOAD_RUSH_CONFIG,
    LOAD_SEPT_CONFIG,
    LOAD_CALL_CONFIG,
    LOAD_RELIVE_CONFIG,
    LOAD_COUNTRY_CONFIG,
    LOAD_NPC_TALK,
    LOAD_SYSTEM_TWITTER,
    LOAD_NPC_TASK,
    LOAD_HORSE,
    LOAD_BRORDCAST,
    LOAD_ACTIVITY,
    LOAD_DEATHPUNISH,
    LOAD_GIFT,
    LOAD_REWARD,
    LOAD_EQUIP_SUIT,
    LOAD_TELE_CONFIG,
    LOAD_ENCOURAGE_CONFIG,
    LOAD_CASHCOW_CONFIG,
    LOAD_VIP_CONFIG,
    LOAD_PARTNER_CONFIG,
    LOAD_FESTIVAL_CONFIG,
    LOAD_WORLD_LEVEL,
    LOAD_EXP_RETRIEVE,
    LOAD_ADDICT_CONFIG,
    LOAD_MUSHOP_CONFIG,
    LOAD_WORLDLEVEL_NPC_CONFIG,
    LOAD_SCSHOP_CONFIG,
    LOAD_DMAPATTR_CONFIG,
    LOAD_EPIC_CONFIG,
    LOAD_MALL_CONFIG,
    LOAD_FATEHUNT_CONFIG,
    LOAD_COMPOSE_CONFIG,
    LOAD_DAILYHELPER_CONFIG,
    LOAD_RICHMAN_CONFIG,

    LOAD_XML_ALL=1999,
    //加载数据库
    LOAD_DB_MEDIA = 2000,
};
enum eRandomNameCode{
    eRan_Success=1,
    eRan_Fail,
    eRan_NoFindLib,
    eRan_MAX
};

#define MAX_NAMESIZE 32
#define MAX_ROLENAMESIZE 18//创建角色名字长度
#define MAX_ROLE_SIZE 3
#define MAX_DBCHAR256_SIZE 256
#define SHA1_LEN 40
#define SERIAL_LEN 36
#define USER_SOURCE_LEN 128
#define CITIZEN_ID_LEN 18        //公民身份证长度
#define CITIZEN_NAME_LEN 20        //公民姓名长度

#define UInt32_STR_LEN 10
#define UInt64_STR_LEN 20

#define MAX_SERVER_CONN 256
#define MAX_SERVER_EVENT 256

#define MAX_BUFFER_LEN  512

#define MAX_BINDATA_SIZE (1<<17)
#define MAX_DBDATA_SIZE (1<<22)

#define WM_LOGG_APPEND  WM_USER+100
#define WM_CHAT         WM_USER+101
#define WM_NCHAT        WM_USER+102
#define WM_UPDATA       WM_USER+103
#define PORT            4009
#define N_PORT          500
#define OP_READ         0
#define OP_WRITE        1
#define MAX_SUBTHREAD_SIZE 5
#define _IsUnused __attribute__ ((__unused__))

#define _TY(x) #x 
#define _S(x) _TY(x)
//随即名字循环最大次数
#define MAX_RAMDONCOUNT 200
//场景id中国家id占的数据起始位
#define SCENE_COUNTRY_BITLEN 28
//场景中地图id
#define REAL_MAP_ID_MASK 0x0FFFFFFF
//进入游戏到选择角色后登录的最大等待时间
#define MAX_GATE_WAIT_TIME 1800
//后台发送数据的最大延时时间
#define MAX_HOUTAI_DELAY_TIME 60
//连接后等待发送消息的最长时间
#define MAX_WAIT_SEND_CMD_TIME 30
//转发用户数据包的最大长度
#define MAX_USER_DATA_SIZE 1024
//踢出玩家等待时间
#define USER_KICK_TIME 120
//变量名的最大长度
#define MAX_VAR_SIZE 16
//邮件最大长度
#define MAX_MAIL_CONTENT_LEN 200
//聊天群名字最大长度
#define MAX_GROUP_NAMESIZE 14
//聊天消息的最大长度
#define MAX_CHAT_LENGTH 255
//群公告最大长度
#define MAX_TWITTER_LENGTH 200
//客服邮件标题最大长度
#define MAX_GMMAIL_TITLE_LEN 64
//客服邮件内容最大长度
#define MAX_GMMAIL_CONTENT_LEN 512
//一屏宽度
#define SCREEN_WIDTH_SIZE 1280
//步长
#define STEP_LEN 50
//TODO
#define MAX_EQUIP_HOLE 5
#define MAIN_MAP_ID 1003

#define DEFAULT_CONFIG_PATH "./Config/"
#define DEFAULT_CONFIG_TABLE_PATH "./Config/Table/SData/"
#define OBJECT_VERSION 1
#define EQUIP_VERSION 2

#ifndef M_PI
#  define M_PI          3.14159265358979323846
#endif

#ifndef M_PI_F
#  define M_PI_F        float(M_PI)
#endif

#ifndef countof
#define countof(array) (sizeof(array) / sizeof((array)[0]))
#endif

#include <string.h>
#include <map>
#include <string>
#include <list>
//#include <stdlib.h>

using namespace std;

#endif
