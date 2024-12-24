#include "ShareMemAO.h"
#include "ShareMemAPI.h"
#include "xBase/xLog.h"

using namespace ShareMemAPI;

enum    CMD_MODE
{
    CMD_MODE_CLEARALL = 1,        //娓呴櫎妯″紡
    CMD_MODE_LOADDUMP = 2,        //杞藉叆dump妯″紡
};

bool    ShareMemAO::Create(SM_KEY key, uint32 Size) {

//     if (m_CmdArg == CMD_MODE_CLEARALL) {
//         return false;
//     }

    m_hold = ShareMemAPI::CreateShareMem(key, Size);
    if (m_hold == INVALID_SM_HANDLE) {
        XLOG(SHMEM_LOG_PATH, "Create ShareMem Error SM_KET = %d", key);
        return false;
    }

    m_pHeader = ShareMemAPI::MapShareMem(m_hold);

    if (m_pHeader) {
        m_pDataPtr = m_pHeader + sizeof(SMHead);
        ((SMHead*)m_pHeader)->m_Key = key;
        ((SMHead*)m_pHeader)->m_Size = Size;
        m_Size = Size;
        XLOG(SHMEM_LOG_PATH, "Create ShareMem Ok SM_KET = %d", key);
        return true;
    } else {
        XLOG(SHMEM_LOG_PATH, "Map ShareMem Error SM_KET = %d", key);
        return false;
    }

    return false;
}

void    ShareMemAO::Destory() {
    if (m_pHeader) {
        ShareMemAPI::UnMapShareMem(m_pHeader);
        m_pHeader = 0;
    }
    if (m_hold) {
        ShareMemAPI::CloseShareMem(m_hold);
        m_hold = 0;
    }

    m_Size = 0;
}

bool    ShareMemAO::Attach(SM_KEY key, uint32    Size) {

    m_hold = ShareMemAPI::OpenShareMem(key, Size);

//     if (m_CmdArg == CMD_MODE_CLEARALL) {
//         Destory();
//         printf("Close ShareMemory key = %d \r\n", key);
//         return false;
//     }

    if (m_hold == INVALID_SM_HANDLE) {

        XLOG(SHMEM_LOG_PATH, "Attach ShareMem Error SM_KET = %d", key);
        return false;
    }

    m_pHeader = ShareMemAPI::MapShareMem(m_hold);

    if (m_pHeader) {
        m_pDataPtr = m_pHeader + sizeof(SMHead);
        assert(((SMHead*)m_pHeader)->m_Key == key);
        assert(((SMHead*)m_pHeader)->m_Size == Size);
        m_Size = Size;
        XLOG(SHMEM_LOG_PATH, "Attach ShareMem OK SM_KET = %d", key);
        return true;
    } else {
        XLOG(SHMEM_LOG_PATH, "Map ShareMem Error SM_KET = %d", key);
        return false;
    }

    return false;
}

void    ShareMemAO::Clean(SM_KEY key) {
    ShareMemAPI::Clean(key);
}

bool    ShareMemAO::DumpToFile(char* FilePath) {

    assert(FilePath);

    FILE* f = fopen(FilePath, "wb");
    if (!f) {
        return false;
    }

    fwrite(m_pHeader, 1, m_Size, f);
    fclose(f);

    return true;
}

bool ShareMemAO::MergeFromFile(char* FilePath) {

    assert(FilePath);

    FILE*    f = fopen(FilePath, "rb");
    if (!f) {
        return false;
    }

    fseek(f, 0L, SEEK_END);
    int FileLength = ftell(f);
    fseek(f, 0L, SEEK_SET);
    fread(m_pHeader, FileLength, 1, f);
    fclose(f);

    return true;
}

void ShareMemAO::SetHeadVer(uint32 ver) {
    ((SMHead*)m_pHeader)->m_HeadVer = ver;
}

uint32 ShareMemAO::GetHeadVer() {
    uint32 ver = ((SMHead*)m_pHeader)->m_HeadVer;
    return ver;
}



