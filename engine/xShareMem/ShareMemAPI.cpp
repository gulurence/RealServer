#include "ShareMemAPI.h"
#include "ShareDef.h"

#ifndef _WIN32
#include <sys/types.h> 
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <errno.h>
#else
#include <WinBase.h>
#endif

namespace ShareMemAPI {

    SMHandle CreateShareMem(SM_KEY key, uint32 Size) {
        //CHAR keybuf[64];
        //memset(keybuf,0,64);
        //sprintf(keybuf,"./%d",key);
#ifndef _WIN32
        //key = ftok(keybuf,'w'); 
        SMHandle hd = shmget(key, Size, IPC_CREAT | IPC_EXCL | 0666);
        XLOG("handle = %d ,key = %d ,error: %d \r\n", hd, key, errno);
        return hd;
#else
        char keybuf[64] = { 0 };
        sprintf(keybuf, "%d", key);
        wchar_t tchar[512] = { 0 };
        //MultiByteToWideChar(CP_ACP, 0, keybuf, -1, tchar, 256 * sizeof(TCHAR));
        return CreateFileMapping((HANDLE)0xFFFFFFFFFFFFFFFF, NULL, PAGE_READWRITE, 0, Size, tchar);
#endif    

        return SMHandle(-1);

    }

    SMHandle OpenShareMem(SM_KEY key, uint32 Size) {
        //CHAR keybuf[64];
        //memset(keybuf,0,64);
        //sprintf(keybuf,"./%d",key);
#ifndef _WIN32
        //key = ftok(keybuf,'w'); 
        SMHandle hd = shmget(key, Size, 0);
        XLOG("handle = %d ,key = %d ,error: %d \r\n", hd, key, errno);
        return hd;
#else
        char keybuf[64] = { 0 };
        sprintf(keybuf, "%d", key);
        wchar_t tchar[512] = { 0 };
        //MultiByteToWideChar(CP_ACP, 0, keybuf, -1, tchar, 256 * sizeof(TCHAR));
        return OpenFileMapping(FILE_MAP_ALL_ACCESS, TRUE, tchar);
#endif
        return SMHandle(-1);
    }

    void Clean(SM_KEY key) {
#ifndef _WIN32
        SMHandle nShmId = shmget(key, 0, 0);
        if (nShmId != -1) {
            shmctl(nShmId, IPC_RMID, 0);
        }
#endif
    }

    char* MapShareMem(SMHandle handle) {

#ifndef _WIN32
        return  (char*)shmat(handle, 0, 0);
#else
        return (char*)MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
#endif    

        return 0;
    }


    void UnMapShareMem(char* MemoryPtr) {
#ifndef _WIN32
        shmdt(MemoryPtr);
#else
        UnmapViewOfFile(MemoryPtr);
#endif    
    }


    void CloseShareMem(SMHandle handle) {
#ifndef _WIN32
        shmctl(handle, IPC_RMID, 0);
#else
        CloseHandle(handle);
#endif    
    }
}



