#pragma once

#include "../xBase/xDefine.h"
#include "xDBMeta.h"
#include "xDBConnPool.h"



class RecordFun
{
public:
    RecordFun() {}
    ~RecordFun() {}

public:
    static uint64 Select(DBConnPool *dbConnPool, const DBConnID &connID, dbCol *pDBCol, char *pCache, const std::string &strTableName, const std::string &strWhere) {
        if (!pDBCol || !dbConnPool) {
            return -1;
        }
        if ((DBConnID)-1 != connID) {
            return dbConnPool->exeSelectEx(connID, strTableName.c_str(), pDBCol, (unsigned char*)pCache, strWhere.c_str());
        }
        return 0;
    }
    static bool Insert(DBConnPool *dbConnPool, const DBConnID &connID, const std::string &strTable, const dbCol * pCol, const uint8 *save) {
        if ((DBConnID)-1 != connID && dbConnPool) {
            uint64 ret = dbConnPool->exeInsert(connID, strTable.c_str(), pCol, save);
            if (UInt64_MAX != ret) {
                return true;
            }
        }
        return false;
    }
    static bool Delete(DBConnPool *dbConnPool, const DBConnID &connID, const std::string &strTableName, const std::string &strSql) {
        if ((DBConnID)-1 != connID && dbConnPool) {
            uint64 ret = dbConnPool->exeSql(connID, strSql.c_str());
            if (UInt64_MAX != ret) {
                return true;
            }
        }
        return false;
    }
    static bool Update(DBConnPool *dbConnPool, const DBConnID &connID, const dbCol *pCol, const unsigned char *pData, const std::string &strTableName, const std::string &strWhere) {
        if (!pData || !dbConnPool) {
            return false;
        }
        if ((DBConnID)-1 != connID) {
            uint64 ret = dbConnPool->exeUpdate(connID, strTableName.c_str(), pCol, (const unsigned char *)pData, strWhere.c_str());
            if (UInt64_MAX != ret) {
                return true;
            }
        }
        return false;
    }
    static uint64 ExeSql(DBConnPool *dbConnPool, const DBConnID &connID, const std::string &strSql) {
        if (!dbConnPool || strSql.empty()) {
            return -1;
        }
        if ((DBConnID)-1 != connID) {
            uint64 ret = dbConnPool->exeSql(connID, strSql.c_str());
            if (UInt64_MAX != ret) {
                return ret;
            }
        }
        return -1;
    }
    static uint64 checkExist(DBConnPool *dbConnPool, const DBConnID &connID, const std::string &strTableName, const std::string &strWhere) {
        if (!dbConnPool || strWhere.empty()) {
            return -1;
        }
        if ((DBConnID)-1 != connID) {
            uint64 ret = dbConnPool->checkExist(connID, strTableName.c_str(), strWhere.c_str());
            if (UInt64_MAX != ret) {
                return ret;
            }
        }
        return -1;
    }
};



