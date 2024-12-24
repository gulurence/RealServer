#pragma once

#if 1
#pragma warning(disable:4099)//LNK4099
#pragma warning(disable:4996)
#pragma warning(disable:4200)
#pragma warning(disable:4355)
#pragma warning(disable:4819)
#endif


#ifdef _WINDOWS

typedef char                int8;
typedef short               int16;
typedef int                 int32;
typedef long long           int64;

typedef unsigned char       uint8;
typedef unsigned short      uint16;
typedef unsigned int        uint32;
typedef unsigned long long  uint64;

#else

typedef char                int8;
typedef short               int16;
typedef int                 int32;
typedef long                int64;

typedef unsigned char       uint8;
typedef unsigned short      uint16;
typedef unsigned int        uint32;
typedef unsigned long       uint64;

#endif

#define INVALID_ID (-1)

#define UE_MIN(x,y) (((x)>(y))?(y):(x))
#define UE_MAX(x,y) (((x)<(y))?(y):(x))


#ifndef _WINDOWS
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <winsock2.h>
#include <windows.h>
#define bzero(p1,p2) memset(p1,0,p2)
#define bcopy(p1,p2,p3) memcpy((void*)p2,(void*)p1,p3)
#define __attribute__(p) ;
#define usleep(t) Sleep(t/1000)
#endif

#ifdef _WINDOWS
#ifndef snprintf
//#define snprintf _snprintf
#endif
#endif


#define UInt8_MIN   ((uint8)0)
#define UInt8_MAX   ((uint8)0xff)               //255
#define UInt16_MIN  ((uint16)0)
#define UInt16_MAX  ((uint16)0xffff)            //65535
#define UInt32_MIN  ((uint32)0)
#define UInt32_MAX  ((uint32)0xffffffff)        //4294967295
#define UInt64_MIN  ((uint64)0)
#define UInt64_MAX  ((uint64)0xffffffffffffffff)   //18446744073709551615


//鏉╃偞甯撮崥搴ｇ搼瀵板懎褰傞柅浣圭Х閹垳娈戦張鈧梹鎸庢闂?
#define MAX_WAIT_SEND_CMD_TIME  30
#define MAX_NAMESIZE            32
#define MAX_BINDATA_SIZE        (1<<17)
#define MAX_DBDATA_SIZE         (1<<22)

#ifndef M_PI
#  define M_PI          3.14159265358979323846
#endif

#ifndef M_PI_F
#  define M_PI_F        float(M_PI)
#endif

#ifndef countof
#define countof(array) (sizeof(array) / sizeof((array)[0]))
#endif

#include "LibraryDef.h"

#include <cstdlib>
#include <deque>
#include <iostream>
#include <utility>

#include <queue>
#include <thread>
#include <shared_mutex>
#include <memory>

#include <map>
#include <string>
#include <vector>
#include <list>
#include <set>

#include <cstring>
#include <sstream>
#include <cassert>
#include <stdarg.h>

#include <stdint.h>
#include <cstdio>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <time.h>

#include <random>
#include <functional>
#include <signal.h>
#include <mutex>
#include <assert.h>



