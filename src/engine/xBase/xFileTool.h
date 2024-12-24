#pragma once

#include "xDefine.h"


inline std::string GMakePathToSort(std::string &path) {
    std::string makeStr = path;
    while (true) {
        size_t pos = makeStr.find("\\./");
        if (pos >= 0) {
            makeStr.replace(pos, 2, "");
        }

        pos = makeStr.find("\\.\\");
        if (pos >= 0) {
            makeStr.replace(pos, 2, "");
        }

        pos = makeStr.find("/./");
        if (pos >= 0) {
            makeStr.replace(pos, 2, "");
        }

        pos = makeStr.find("/.\\");
        if (pos >= 0) {
            makeStr.replace(pos, 2, "");
        }

        size_t i1 = makeStr.find("\\./");
        size_t i2 = makeStr.find("\\.\\");
        size_t i3 = makeStr.find("/./");
        size_t i4 = makeStr.find("/.\\");

        if (i1 < 0 && i2 < 0 && i3 < 0 && i4 < 0) {
            break;
        }
    }

    do {
        size_t pos = makeStr.find("..");
        if (pos > 0) {
            // /a/b/c/../sdf/ad.exe
            std::string leftStr = makeStr.substr(0, pos - 1);// /a/b/c
            std::string rightStr = makeStr.substr(pos + 2, makeStr.size());// /sdf/ad.exe

            for (size_t i = leftStr.size() - 1; i >= 0; --i) {
                if (leftStr.at(i) == '\\' || leftStr.at(i) == '/') {
                    makeStr = leftStr.substr(0, i) + rightStr;// /a/b + /sdf/ad.exe
                    break;
                }
            }
        }

        pos = makeStr.find("..");
        if (pos <= 0) {
            break;
        }

    } while (true);

    path = makeStr;
    makeStr = "";

    for (size_t i = 0; i < path.size(); ++i) {
        if ('/' == path.at(i)) {
            makeStr += '\\';
        } else {
            makeStr += path.at(i);
        }
    }

    return path = makeStr;
}

inline std::string GMakeCurProcessRelativePath(const std::string &strRelativePath) {
    //std::wstring mLauncherBasePath;

#ifdef _WIN32
    //wchar_t runPath[MAX_PATH] = { 0 };
    //GetModuleFileName(NULL, runPath, MAX_PATH);

    ////char nchar[256] = { 0 };
    ////WideCharToMultiByte(CP_ACP, 0, runPath, -1, nchar, 256, NULL, NULL);

    //mLauncherBasePath = runPath;
    //mLauncherBasePath = mLauncherBasePath.substr(0, mLauncherBasePath.rfind('\\') + 1);
    //mLauncherBasePath = mLauncherBasePath + strRelativePath;
    //return GMakePathToSort(mLauncherBasePath);
    return "";
#else

    return relativePath;

#endif
}


inline std::string GMakeCurProcessRelativePathEx(const std::string &basePath, const std::string &relativePath) {
    if (basePath.empty()) {
        return GMakeCurProcessRelativePath(relativePath);
    }


#ifdef _WIN32
    std::string mLauncherBasePath;
    mLauncherBasePath = basePath;
    mLauncherBasePath = mLauncherBasePath.substr(0, mLauncherBasePath.rfind('\\') + 1);
    // 缂佹繂顕捄顖氱窞閸滃瞼娴夌€电鐭惧鍕閸掗绔寸挧?
    mLauncherBasePath = mLauncherBasePath + relativePath;
    // 閸樼粯甯€鐠侯垰绶炴稉顓犳畱閻╃顕捄顖氱窞 ".." 閸?"."
    return GMakePathToSort(mLauncherBasePath);
#else
    
    return relativePath;

#endif

}

inline bool GSetProcessRunPath(const char * pszPath) {
    if (!pszPath) {
        return false;
    }

#ifdef _WIN32

    if (!SetCurrentDirectoryA(pszPath)) {
        char tempLog[512] = { 0 };
        sprintf_s(tempLog, "[%s] 鐠佸墽鐤嗛幍褑顢戦惄顔肩秿闁挎瑨顕?!!!\n", pszPath);
        wchar_t tchar[512] = { 0 };
        MultiByteToWideChar(CP_ACP, 0, tempLog, -1, tchar, 256 * sizeof(TCHAR));
        //MessageBox(nullptr, tchar, "閹绘劗銇?!! ", 0);
        return false;
    }

#else

    bool ret = chdir(pszPath);
    if (ret != 0) {
        XERR(" ZoneServer::setRunBasePath chdir error [%s] \n", pszPath);
        return false;
    }

    char curCwd[256] = { 0 };
    getcwd(curCwd, 255);
    XLOG("Cur Linux Run Path : %s ", curCwd);

#endif

    printf("set run path [%s] success !!! \n", pszPath);
    return true;
}









