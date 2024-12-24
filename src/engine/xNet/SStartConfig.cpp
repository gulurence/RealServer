#include "SStartConfig.h"
#include "../xBase/xFileTool.h"
#include "../xBase/UTF8.h"
#include "../xSconnect/GameServerRunShareMgr.h"
//#include "../xServer/ZoneServer.h"


#ifdef _WIN32
#include <tlhelp32.h>
#include <Shlwapi.h>
#include <comdef.h>
#else
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

void SServerStartConfig::PrintBaseConfig() {
    mServerNodeFullConfig.base_config.Printf();
    mServerNodeFullConfig.PrintReqConnectInfo();
}

uint32 SServerStartConfig::GetLauncherSGUID() {
    for (auto it:mServerNodeFullConfig.req_connects) {
        if ("SLauncher" == it.second.sname) {
            return it.second.sguid.getSGUID();
        }
    }

    return 0;
}

bool SServerStartConfig::ProcessAlreadyRunning(const std::string &sName, uint32 sguid) {
    const char *nodePtr = GameServerRunShareMgr::getMe().GetNodePtr(sguid);
    if (!nodePtr) {
        XERR("SServerStartConfig::ProcessAlreadyRunning - !nodePtr [sguid:%d]", sguid);
        return false;
    }

    SMemStruct *pSMemStruct = (SMemStruct *)nodePtr;
    
#ifdef _WIN32
    
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == hSnapshot) {
        return true;
    }

    SGUID_UINT32 stSGUID(sguid);

    // 閫氳繃鏌ユ壘杩涚▼鍚嶇О鏉ョ‘璁ゆ槸鍚﹀惎鍔?
    wchar_t tempProceessName[128] = { 0 };
    wsprintf(tempProceessName, L"SG_%s_G%d_N%d.exe", sName.c_str(), stSGUID.getGroupID(), stSGUID.getNodeID());

    PROCESSENTRY32 pi;
    pi.dwSize = sizeof(PROCESSENTRY32);
    BOOL bRet = Process32First(hSnapshot, &pi);
    while (bRet) {

        std::wstring findPName = pi.szExeFile;// GGbkToUtf8(pi.szExeFile);
        //XLOG(" --->>> ProcessAlreadyRunning - %s ", findPName.c_str());
        if (findPName == tempProceessName && pi.th32ProcessID != GetCurrentProcessId()) {

            return true;
        }

        bRet = Process32Next(hSnapshot, &pi);
    }

    return false;

#else

    char proc_pid_path[256] = { 0 };
    char proc_name_tmp[128] = { 0 };
    sprintf(proc_pid_path, "/proc/%lld/status", pSMemStruct->u64PID);
    FILE* fp = fopen(proc_pid_path, "r");
    if (NULL != fp) {
        if (fgets(proc_name_tmp, 128 - 1, fp) == NULL) {
            fclose(fp);
        }

        XLOG("1111 ++++++++++++++++++ ProcessAlreadyRunning +++++++++++++++++ %lld  szProcessName:%s proc_name_tmp:%s ", pSMemStruct->u64PID, pSMemStruct->szProcessName, proc_name_tmp);

        string proc_info = proc_name_tmp;
        if (proc_info.find(pSMemStruct->szProcessName) >= 0) {
            return true;
        }
    }

    XLOG("2222 ++++++++++++++++++ ProcessAlreadyRunning +++++++++++++++++ %lld ", pSMemStruct->u64PID);

    return false;

#endif

}

bool SServerStartConfig::InitGame(int argc, char *argv[]) {

    SGUID_UINT32 sguid(atoi(argv[1]));
    const std::string &basePath = argv[0];

    for (int i = 0;i< argc;++i) {
        printf(" SServerStartConfig::InitGame ------------------------ param %d [%s]  \n", i, argv[i]);
    }

    // 鍏变韩鍐呭瓨鍙傛暟
    if (sguid.getServerID() <= 0) {
        printf("SServerStartConfig::InitGame - sguid <= 0 [sguid:%d] \n", sguid.getSGUID());
        return false;
    }

#ifdef _WIN32
    // 鎵嬪姩鍒涘缓妯″紡 璁剧疆 
    // 杩欓噷涓嶉渶瑕佽缃洰褰?鐩存帴鍦ㄦ湰鐩綍杩愯 
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "auto_run") == 0) {
            SetManuallyRun(true);
            break;
        }
    }
#endif

    // 鍏堟竻闄ゆ帀鍏变韩鍐呭瓨 閲嶅惎鎵嶈兘鍒犻櫎 
    //GameServerRunShareMgr::getMe().Clean(SHARE_MEM_KEY_ID); 

    printf("SServerStartConfig::InitGame - Begin Attach [SHARE_MEM_KEY_ID:%d,DEFAULT_NODE_COUNT:%d,DEFAULT_NODE_SIZE:%d]  \n", SHARE_MEM_KEY_ID, DEFAULT_NODE_COUNT, DEFAULT_NODE_SIZE);

    // 鑾峰彇鍏变韩鍐呭瓨閰嶇疆鏁版嵁 
    if (!GameServerRunShareMgr::getMe().Attach(SHARE_MEM_KEY_ID, DEFAULT_NODE_COUNT*DEFAULT_NODE_SIZE)) {
        printf("SServerStartConfig::InitGame - GameServerRunShareMgr::getMe().Attach [sguid:%d] \n", sguid.getSGUID());
        return false;
    }

    printf("SServerStartConfig::InitGame - [sguid:%d]  \n", sguid.getSGUID());

    // 鍒濆鍖栭厤缃?
    if (!InitConfig(sguid.getSGUID())) {
        printf("SServerStartConfig::InitGame - !nodePtr || iNodexSize <= 0 [sguid:%d] \n", sguid.getSGUID());
        return false;
    }

    printf("SServerStartConfig::InitGame - begin GetManuallyRun \n");

    std::string strServerName = GetServerName();
    // 杩欓噷闇€瑕佽缃笅杩愯鐨勭洰褰?auto_run 鏄?SLauncher 鍚姩鐨?
    if(GetManuallyRun()) {
        char tempRunPath[1024] = { 0 };
        sprintf(tempRunPath,"../../SGame/%s/G%d_N%d/", strServerName.c_str(), sguid.getGroupID(), sguid.getNodeID());
        GSetProcessRunPath(tempRunPath);
    }

    printf("SServerStartConfig::InitGame - begin new xLog [strServerName:%s] \n", strServerName.c_str());

    // xLog
    //g_pSrvLog = new xLog(strServerName.c_str());
    //if (!g_pSrvLog || !g_pSrvLog->init(strServerName.c_str())) {
    //    printf("[%s] g_pSrvLog init failed \n", strServerName.c_str());
    //    return false;
    //}

#ifdef _WIN32

    if (!GetManuallyRun()) {
        // 鍚姩妫€鏌?
        if (ProcessAlreadyRunning(strServerName, sguid.getSGUID())) {
            XERR("SServerStartConfig::InitGame - ProcessAlreadyRunning(sguid) [sguid:%d] ", sguid);
            return false;
        }
    }

#endif
    
    return true;
}

bool SServerStartConfig::InitConfig(uint32 sguid) {

    const char *nodePtr = GameServerRunShareMgr::getMe().GetNodePtr(sguid);
    if (!nodePtr) {
        XERR("SServerStartConfig::InitConfig - !nodePtr || iNodexSize <= 0 [shareMemIndex:%d]", sguid);
        return false;
    }

    short iPos = 0;
    SMemStruct *pSMemStruct = (SMemStruct *)nodePtr;
    bool ret = mServerNodeFullConfig.UnSerialize(pSMemStruct->pNodeFullConfigData, iPos, pSMemStruct->pNodeFullConfigDataLen);
    //mServerNodeFullConfig.PrintReqConnectInfo();
    return ret;
}







