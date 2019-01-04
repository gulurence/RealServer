#pragma once
#include "xDefine.h"
#include "xDBMeta.h"
#include <mysql.h>
#include <map>
#include "xUniqueIDManager.h" 
#include "xFunctionTime.h"

typedef UInt8 DBConnID;
#define DBErrConnID ((DBConnID)-1)
#define DBErrReturn UInt64(-1)
//数据库连接
class DBConn
{
    friend class DBConnPool;
    public:
        DBConn();
        ~DBConn()
        {
            mysql_close(mysql);   
        }
        bool reconnect(const char *server, const char *user, const char *password, const char *database);
        bool init(const char *server, const char *user, const char *password, const char *database);
        void close();
        UInt64 exeInsert(const char *tableName, const dbCol *column, const unsigned char *data, bool isRet);
        UInt64 exeInsertMany(const char *tableName, const dbCol *column, const unsigned char *data, UInt32 num,const char *where);
        UInt64 exeUpdate(const char *tableName, const dbCol *column, const unsigned char *data, const char *where);
        UInt64 exeSelect(const char *tableName, const dbCol *column, unsigned char **data, const char *where, const char *extraOpt);
        UInt64 exeDelete(const char *table, const char *where);
        UInt64 exeSql(const char *sql);
        UInt64 checkExist(const char *table, const char *where);
        UInt64 getNum(const char *table,const char *where);

    private:
        MYSQL *mysql;
};
//连接池
class DBConnPool
{
    public:
        DBConnPool()
        {
            _max = 0;
            pool.clear();
            ids = NULL;
            _server.clear();
            _user.clear();
            _password.clear();
            _database.clear();
        }
        bool init();
        bool loadConfig(const char *serverName);
        DBConnID getDBConn();
        void putDBConn(DBConnID con);

        UInt64 exeInsertMany(DBConnID id, const char *tableName, const dbCol *column, const unsigned char *data, UInt32 num,const char* where=NULL)
        {
            FunctionTime_Scope;
            if (pool.find(id)!=pool.end() && pool[id] && pool[id]->reconnect(_server.c_str(), _user.c_str(), _password.c_str(), _database.c_str()))
                return pool[id]->exeInsertMany(tableName, column, data, num,where);
            return DBErrReturn;
        }
        UInt64 exeInsert(DBConnID id, const char *tableName, const dbCol *column, const unsigned char *data, bool isRet = true)
        {
            FunctionTime_Scope;
            if (pool.find(id)!=pool.end() && pool[id] && pool[id]->reconnect(_server.c_str(), _user.c_str(), _password.c_str(), _database.c_str()))
                return pool[id]->exeInsert(tableName, column, data, isRet);
            return DBErrReturn;
        }
        UInt64 exeUpdate(DBConnID id, const char *tableName, const dbCol *column, const unsigned char *data, const char *where)
        {
            FunctionTime_Scope;
            if (pool.find(id)!=pool.end() && pool[id] && pool[id]->reconnect(_server.c_str(), _user.c_str(), _password.c_str(), _database.c_str()))
                return pool[id]->exeUpdate(tableName, column, data, where);
            return DBErrReturn;
        }
        UInt64 exeSelect(DBConnID id, const char *tableName, const dbCol *column, unsigned char **data, const char *where, const char *extraOpt = NULL)
        {
            FunctionTime_Scope;
            if (pool.find(id)!=pool.end() && pool[id] && pool[id]->reconnect(_server.c_str(), _user.c_str(), _password.c_str(), _database.c_str()))
                return pool[id]->exeSelect(tableName, column, data, where, extraOpt);
            return DBErrReturn;
        }
        UInt64 exeDelete(DBConnID id, const char *tableName, const char *where)
        {
            FunctionTime_Scope;
            if (pool.find(id)!=pool.end() && pool[id] && pool[id]->reconnect(_server.c_str(), _user.c_str(), _password.c_str(), _database.c_str()))
                return pool[id]->exeDelete(tableName, where);
            return DBErrReturn;
        }
        UInt64 exeSql(DBConnID id, const char *sql)
        {
            FunctionTime_Scope;
            if (pool.find(id)!=pool.end() && pool[id] && pool[id]->reconnect(_server.c_str(), _user.c_str(), _password.c_str(), _database.c_str()))
                return pool[id]->exeSql(sql);
            return DBErrReturn;
        }
        UInt64 checkExist(DBConnID id, const char *tableName, const char *where)
        {
            FunctionTime_Scope;
            if (pool.find(id)!=pool.end() && pool[id] && pool[id]->reconnect(_server.c_str(), _user.c_str(), _password.c_str(), _database.c_str()))
                return pool[id]->checkExist(tableName, where);
            return DBErrReturn;
        }
        UInt64 getNum(DBConnID id, const char *tableName, const char *where)
        {
            FunctionTime_Scope;
            if (pool.find(id)!=pool.end() && pool[id] && pool[id]->reconnect(_server.c_str(), _user.c_str(), _password.c_str(), _database.c_str()))
                return pool[id]->getNum(tableName, where);
            return DBErrReturn;
        }
        void freeID();

    private:
        ~DBConnPool();

        DBConnID _max;
        std::string _server;
        std::string _user;
        std::string _password;
        std::string _database;

        std::map<DBConnID, DBConn *> pool;
        typedef std::map<DBConnID, DBConn *>::iterator pool_iter;

        UniqueIDManager<DBConnID> *ids;
};
