#include "xDBConnPool.h"

#include "../xBase/xTools.h"
#include "../xBase/xXMLParser.h"
#include "../xBase/xStringTool.h"
#include <sstream>
#include <mysql/mysql.h>


#define TO_LOWER_STR(str) StringTool::tolower(str)

DBConn::DBConn()
{
    mysql = NULL;
}

bool DBConn::init(const char *server, const char *user, const char *password, const char *database)
{
    mysql = mysql_init(NULL);

    //char value = 1;
    //mysql_options(mysql, MYSQL_OPT_RECONNECT, (char *)&value);

    /* Connect to database */
    if (!mysql_real_connect(mysql, server, user, password, database, 0, NULL, 0))
    {
        XERR("[MYSQL]涓嶈兘杩炴帴mysql server:%s, user:%s, password:%s, database:%s", server, user, password, database);
        return false;
    }
    mysql_query(mysql, "SET NAMES utf8");
    XDBG("connect mysql server:%s,user:%s,password:%s,database:%s success",server,user,password,database);
    return true;
}

bool DBConn::reconnect(const char *server, const char *user, const char *password, const char *database)
{
    if (mysql_ping(mysql)) 
    {
//        mysql_close(mysql); // 鍏堝叧闂潪姝ｅ父杩炴帴

        if (!mysql_real_connect(mysql, server, user, password, database, 0, NULL, 0))
        {
            XERR("[MYSQL]涓嶈兘杩炴帴mysql server:%s, user:%s, password:%s, database:%s", server, user, password, database);
            return false;
        }
        mysql_set_character_set(mysql, "utf8");
        //mysql_query(mysql, "SET NAMES utf8");
    }
    return true;
}

void DBConn::close()
{
}

uint64 DBConn::exeSql(const char *sql)
{
    if (sql == NULL) return DBErrReturn;

    if (mysql_real_query(mysql, sql, strlen(sql)))   
    {   
        XERR("[MYSQL],%s", sql);
        return DBErrReturn;
    }   
    return mysql_affected_rows(mysql);
}

uint64 DBConn::exeDelete(const char *table, const char *where)
{
    if (NULL == table) return DBErrReturn;

    std::string lowTable(table);
    std::stringstream query_string;
    query_string.str("");
    query_string << "DELETE FROM " << TO_LOWER_STR(lowTable);
    if (NULL != where)
        query_string << " WHERE " << where;
#ifdef _SQL_DEBUG_ZYX
    XDBG("[MYSQL DELETE]:%s", query_string.str().c_str());
#endif
    /* send SQL query */  
    if (mysql_real_query(mysql, query_string.str().c_str(), query_string.str().size()))   
    {   
        XERR("[MYSQL],%s", query_string.str().c_str());
        return DBErrReturn;
    }   
    return mysql_affected_rows(mysql);
}

uint64 DBConn::exeUpdate(const char *tableName, const dbCol *column, const unsigned char *data, const char *where)
{
    std::stringstream sql_string;
    std::string lowTable(tableName);
    sql_string.str("");
    sql_string << "UPDATE " << TO_LOWER_STR(lowTable) << " SET ";
    uint32 k = 0;
    uint32 place = 0;
    std::string value;
    bool isHaveBin = false;
    while (0!=strncasecmp(column[k].name,"NULL",MAX_NAMESIZE))
    {
        if (k)
            sql_string << ",";
        value.clear();
        sql_string << column[k].name << "=";
        switch (column[k].type)
        {
            case DBDATA_TYPE_CHAR:
                {
                    sql_string << "'" << (char *)(&data[place]) << "'";
                    place += column[k].size;
                }
                break;
            case DBDATA_TYPE_UInt8:
                {
                    sql_string << *(uint8 *)(&data[place]);
                    place += column[k].size;
                }
                break;
            case DBDATA_TYPE_INT:
                {
                    sql_string << *(int *)(&data[place]);
                    place += column[k].size;
                }
                break;
            case DBDATA_TYPE_UInt16:
                {
                    sql_string << *(uint16 *)(&data[place]);
                    place += column[k].size;
                }
                break;
            case DBDATA_TYPE_UInt32:
                {
                    sql_string << *(uint32 *)(&data[place]);
                    place += column[k].size;
                }
                break;
            case DBDATA_TYPE_UInt64:
                {
                    sql_string << *(uint64 *)(&data[place]);
                    place += column[k].size;
                }
                break;
            case DBDATA_TYPE_BIN:
                {
                    char *tmp = new char[column[k].size*2]; 
                    bzero(tmp, column[k].size*2);
                    mysql_real_escape_string(mysql,(char *)tmp, (char *)&data[place], column[k].size);
#ifdef _SQL_DEBUG
                    XDBG("[MYSQL BIN STR]sql1:%u, size:%u, tmpsize:%u", sql_string.str().size(), column[k].size, strlen(tmp));
#endif
                    sql_string << "\'" << tmp << "\'";
                    isHaveBin = true;
#ifdef _SQL_DEBUG 
                    XDBG("[MYSQL BIN STR]sql2:%u", sql_string.str().size());
#endif
                    place += column[k].size;

                    delete []tmp;
                    tmp = NULL;
                }
                break;
            case DBDATA_TYPE_BIN2:
                {
                    uint32 size = *(uint32 *)&data[place];
                    
                    if (size > 0) {
                        char *tmp = new char[(size + sizeof(uint32)) * 2];
                        bzero(tmp, (size + sizeof(uint32)) * 2);
                        mysql_real_escape_string(mysql, (char *)tmp, (char *)&data[place], size + sizeof(uint32));
#ifdef _SQL_DEBUG
                        XDBG("[MYSQL BIN2 STR]sql1:%u, size:%u, tmpsize:%u", sql_string.str().size(), size, strlen(tmp));
#endif
                        sql_string << "\'" << tmp << "\'";
                        isHaveBin = true;
#ifdef _SQL_DEBUG
                        XDBG("[MYSQL BIN2 STR]sql2:%u", sql_string.str().size());
#endif
                        delete[]tmp;
                        tmp = NULL;
                    }
                    else {
                        sql_string << "\'" << "" << "\'";
                    }

                    place += (size+sizeof(uint32));

                }
                break;
            default:
                break;
        }
#ifdef _SQL_DEBUG
        XDBG("[MYSQL STR]column type:%u, size:%u, value:%s", column[k].type, column[k].size, value.c_str());
#endif
        k++;
    }
    if (NULL != where)
        sql_string << " WHERE " << where;
#ifdef _SQL_DEBUG_ZYX
    //if(!isHaveBin)
    //XDBG("[MYSQL UPDATE],%s", sql_string.str().c_str());
#endif
    /* send SQL query */ 
    std::string strSqlInfo = sql_string.str().c_str();
    if (mysql_real_query(mysql, strSqlInfo.c_str(), strSqlInfo.size()))
    {   
        XERR("[MYSQL],%s", sql_string.str().c_str());
        return DBErrReturn;
    }   
    return mysql_affected_rows(mysql);
}

uint64 DBConn::exeInsert(const char *tableName, const dbCol *column, const unsigned char *data, bool isRet)
{
    std::stringstream sql_string;
    sql_string.str("");
    std::string lowTable(tableName);
    sql_string << "INSERT INTO " << TO_LOWER_STR(lowTable) << " (";
    uint32 k = 0;
    while (0!=strncasecmp(column[k].name,"NULL",MAX_NAMESIZE))
    {
        if (k)
            sql_string << ",";
        sql_string << column[k].name;
        k++;
    }
    sql_string << ") VALUES(";
    k = 0;
    uint32 place = 0;
    std::string value;
    while (0!=strncmp(column[k].name,"NULL",MAX_NAMESIZE))
    {
        if (k)
            sql_string << ",";
        value.clear();
        switch (column[k].type)
        {
            case DBDATA_TYPE_CHAR:
                {
                    sql_string << "'" << (char *)(&data[place]) << "'";
                    place += column[k].size;
                }
                break;
            case DBDATA_TYPE_UInt8:
                {
                    sql_string << *(uint8 *)(&data[place]);
                    place += column[k].size;
                }
                break;
            case DBDATA_TYPE_INT:
                {
                    sql_string << *(int *)(&data[place]);
                    place += column[k].size;
                }
                break;
            case DBDATA_TYPE_UInt16:
                {
                    sql_string << *(uint16 *)(&data[place]);
                    place += column[k].size;
                }
                break;
            case DBDATA_TYPE_UInt32:
                {
                    sql_string << *(uint32 *)(&data[place]);
                    place += column[k].size;
                }
                break;
            case DBDATA_TYPE_UInt64:
                {
                    sql_string << *(uint64 *)(&data[place]);
                    place += column[k].size;
                }
                break;
            case DBDATA_TYPE_BIN:
                {
                    char *tmp = new char[column[k].size*2]; 
                    bzero(tmp, column[k].size*2);
                    mysql_real_escape_string(mysql,(char *)tmp, (char *)&data[place], column[k].size);
#ifdef _SQL_DEBUG
                    XDBG("[MYSQL BIN STR]sql1:%u, size:%u, tmpsize:%u", sql_string.str().size(), column[k].size, strlen(tmp));
#endif
                    sql_string << "\'" << tmp << "\'";
#ifdef _SQL_DEBUG
                    XDBG("[MYSQL BIN STR]sql2:%u", sql_string.str().size());
#endif
                    place += column[k].size;

                    delete []tmp;
                    tmp = NULL;
                }
                break;
            case DBDATA_TYPE_BIN2:
                {
                    uint32 size = *(uint32 *)&data[place];
                    if (size>0) {
                        char *tmp = new char[(size + sizeof(uint32)) * 2];
                        bzero(tmp, (size + sizeof(uint32)) * 2);
                        mysql_real_escape_string(mysql, (char *)tmp, (char *)&data[place], size + sizeof(uint32));
#ifdef _SQL_DEBUG
                        XDBG("[MYSQL BIN2 STR]sql1:%u, size:%u, tmpsize:%u", sql_string.str().size(), size, strlen(tmp));
#endif
                        sql_string << "\'" << tmp << "\'";
#ifdef _SQL_DEBUG
                        XDBG("[MYSQL BIN2 STR]sql2:%u", sql_string.str().size());
#endif
                        delete[]tmp;
                        tmp = NULL;
                    }
                    else {
                        sql_string << "\'" << "" << "\'";
                    }

                    place += (size+sizeof(uint32));

                }
                break;
            default:
                break;
        }
#ifdef _SQL_DEBUG
        XDBG("[MYSQL INSERT STR]column type:%u, size:%u, value:%s", column[k].type, column[k].size, value.c_str());
#endif
        k++;
    }
    sql_string << ")" ;
#if defined(_SQL_DEBUG_ZYX)
    printf("[MYSQL INSERT],%s \n", sql_string.str().c_str());
#endif
    /* send SQL query */  
    if (mysql_real_query(mysql, sql_string.str().c_str(), sql_string.str().size()))   
    {   
        XERR("[MYSQL]query error! %s", sql_string.str().c_str());
        return DBErrReturn;
    }   
    if (isRet)
        return mysql_insert_id(mysql);
    else
        return mysql_affected_rows(mysql);
}

uint64 DBConn::exeInsertMany(const char *tableName, const dbCol *column, const unsigned char *data, uint32 num,const char* where)
{
    std::stringstream sql_string;
    sql_string.str("");
    std::string lowTable(tableName);
    sql_string << "INSERT INTO " << TO_LOWER_STR(lowTable) << " (";
    uint32 k = 0;
    while (0!=strncasecmp(column[k].name,"NULL",MAX_NAMESIZE))
    {
        if (k)
            sql_string << ",";
        sql_string << column[k].name;
        k++;
    }
    sql_string << ") VALUES";

    uint32 place = 0;
    std::string value;
    for(uint32 recordnum=0;recordnum<num;recordnum++)
    {
        sql_string <<"(";
        k = 0;
        while (0!=strncmp(column[k].name,"NULL",MAX_NAMESIZE))
        {
            if (k)
                sql_string << ",";
            value.clear();
            switch (column[k].type)
            {
                case DBDATA_TYPE_CHAR:
                    {
                        sql_string << "'" << (char *)(&data[place]) << "'";
                        place += column[k].size;
                    }
                    break;
                case DBDATA_TYPE_UInt8:
                    {
                        sql_string << *(uint8 *)(&data[place]);
                        place += column[k].size;
                    }
                    break;
                case DBDATA_TYPE_INT:
                    {
                        sql_string << *(int *)(&data[place]);
                        place += column[k].size;
                    }
                    break;
                case DBDATA_TYPE_UInt16:
                    {
                        sql_string << *(uint16 *)(&data[place]);
                        place += column[k].size;
                    }
                    break;
                case DBDATA_TYPE_UInt32:
                    {
                        sql_string << *(uint32 *)(&data[place]);
                        place += column[k].size;
                    }
                    break;
                case DBDATA_TYPE_UInt64:
                    {
                        sql_string << *(uint64 *)(&data[place]);
                        place += column[k].size;
                    }
                    break;
                case DBDATA_TYPE_BIN:
                    {
                        char *tmp = new char[column[k].size*2]; 
                        bzero(tmp, (column[k].size*2));
                        mysql_real_escape_string(mysql,(char *)tmp, (char *)&data[place], column[k].size);
#ifdef _SQL_DEBUG
                        XDBG("[MYSQL BIN STR]sql1:%u, size:%u, tmpsize:%u", sql_string.str().size(), column[k].size, strlen(tmp));
#endif
                        sql_string << "\'" << tmp << "\'";
#ifdef _SQL_DEBUG
                        XDBG("[MYSQL BIN STR]sql2:%u", sql_string.str().size());
#endif
                        place += column[k].size;

                        delete []tmp;
                        tmp = NULL;
                    }
                    break;
                case DBDATA_TYPE_BIN2:
                    {
                        uint32 size = *(uint32 *)&data[place];

                        char *tmp = new char[(size+sizeof(uint32))*2]; 
                        memset(tmp,0,(size+sizeof(uint32))*2);
                        mysql_real_escape_string(mysql,(char *)tmp, (char *)&data[place], size+sizeof(uint32));
#ifdef _SQL_DEBUG
                        XDBG("[MYSQL BIN2 STR]sql1:%u, size:%u, tmpsize:%u", sql_string.str().size(), size, strlen(tmp));
#endif
                        sql_string << "\'" << tmp << "\'";
#ifdef _SQL_DEBUG
                        XDBG("[MYSQL BIN2 STR]sql2:%u", sql_string.str().size());
#endif
                        place += (size+sizeof(uint32));

                        delete []tmp;
                        tmp = NULL;
                    }
                    break;
                default:
                    break;
            }
#ifdef _SQL_DEBUG
            XDBG("[MYSQL INSERT STR]column type:%u, size:%u, value:%s", column[k].type, column[k].size, value.c_str());
#endif
            k++;
        }
        if((recordnum+1)==num)
            sql_string << ")" ;
        else
            sql_string << ")," ;
    }
    
    if(where)
        sql_string << where;
#if defined(_SQL_DEBUG_ZYX)
    XDBG("[MYSQL INSERT],%s", sql_string.str().c_str());
#endif
    /* send SQL query */
//    const char * sqlstr=sql_string.str().c_str();
//    printf("sql:%s\n",sqlstr);
    if (mysql_real_query(mysql, sql_string.str().c_str(), sql_string.str().size()))   
    {   
        XERR("[MYSQL]query error! %u,%s", sql_string.str().size(),sql_string.str().c_str());
        return DBErrReturn;
    }   
    return mysql_affected_rows(mysql);
}

uint64 DBConn::exeSelect(const char *tableName, const dbCol *column, unsigned char **data, const char *where, const char *extraOpt)
{
    std::stringstream query_string;
    query_string.str("");
    query_string << "SELECT ";
    uint32 k = 0;
    while (0!=strncasecmp(column[k].name,"NULL",MAX_NAMESIZE))
    {
        if (k)
            query_string << ",";
        query_string << column[k].name;
        k++;
    }
    std::string lowTable(tableName);
    query_string << " FROM " << TO_LOWER_STR(lowTable);
    if (NULL != where)
        query_string << " WHERE " << where;
    if (NULL != extraOpt)
        query_string << " " << extraOpt;
#if defined(_SQL_DEBUG_ZYX)
    XDBG("[MYSQL SELECT],%s", query_string.str().c_str());
#endif
    MYSQL_RES *res;   
    MYSQL_ROW row;   

    /* send SQL query */  
    if (mysql_real_query(mysql, query_string.str().c_str(), query_string.str().size()))   
    {   
        XERR("[MYSQL]query error! %s", query_string.str().c_str());
        return DBErrReturn;
    }   
    uint32 count = 0, place = 0;
    res = mysql_store_result(mysql);   
    int num_fields = mysql_num_fields(res);   

    *data = new unsigned char[MAX_DBDATA_SIZE];
    memset(*data,0x00,MAX_DBDATA_SIZE);
    unsigned long *lengths;
    while ((row = mysql_fetch_row(res)) != NULL)   
    {
        lengths = mysql_fetch_lengths(res);
        for (int i=0; i<num_fields; i++)
        {
            place = count;
            count += column[i].size;
#ifdef _SQL_DEBUG
            XDBG("[MYSQL]mysql_fetch_row total:%lu, lengths[%u]:%lu", count, i, place);
#endif
            if (MAX_DBDATA_SIZE <= count)
            {   
                XERR("[MYSQL],%s鏌ヨ鏁版嵁澶у皬瓒呰繃闄愬埗!",tableName);
                SAFE_DELETE_VEC(*data);
                mysql_free_result(res);   
                return DBErrReturn;
            }   
            switch (column[i].type)
            {
                case DBDATA_TYPE_CHAR:
                    {
                        if ((unsigned char *)row[i]) {
                            bcopy((unsigned char *)row[i], (unsigned char *)(&(*data)[place]), column[i].size);
                        }
                    }
                    break;
                case DBDATA_TYPE_UInt8:
                    {
                        *(uint8 *)(&(*data)[place]) = atoi(row[i]);
                    }
                    break;
                case DBDATA_TYPE_INT:
                    {
                        *(int *)(&(*data)[place]) = atoi(row[i]);
                    }
                    break;
                case DBDATA_TYPE_UInt16:
                    {
                        *(uint16 *)(&(*data)[place]) = atoi(row[i]);
                    }
                    break;
                case DBDATA_TYPE_UInt32:
                    {
                        *(uint32 *)(&(*data)[place]) = atoi(row[i]);
                    }
                    break;
                case DBDATA_TYPE_UInt64:
                    {
                        *(uint64 *)(&(*data)[place]) = atoll(row[i]);
                    }
                    break;
                case DBDATA_TYPE_BIN:
                    {
                        if ((unsigned char *)row[i]) {
                            bcopy((unsigned char *)row[i], (unsigned char *)(&(*data)[place]), column[i].size);
                        }
                    }
                    break;
                case DBDATA_TYPE_BIN2:
                    {
                        count += lengths[i];
                        if (MAX_DBDATA_SIZE <= count)
                        {   
                            XERR("[MYSQL],%s鏌ヨ鏁版嵁澶у皬瓒呰繃闄愬埗!",tableName);
                            SAFE_DELETE_VEC(*data);
                            mysql_free_result(res);   
                            return DBErrReturn;
                        }   
                        if (lengths[i])
                            bcopy((unsigned char *)row[i], (unsigned char *)(&(*data)[place]), lengths[i]);
                        else
                        {
                            *(uint32 *)(&(*data)[place]) = 0;
                            count+=sizeof(uint32);
                        }
                    }
                    break;
                default:
                    break;
            }
        }
    }
    int ret = (int)mysql_num_rows(res);

    mysql_free_result(res);   
    return ret;
}

uint64 DBConn::exeSelectEx(const char *tableName, const dbCol *column, unsigned char *data, const char *where, const char *extraOpt) {
    std::stringstream query_string;
    query_string.str("");
    query_string << "SELECT ";
    uint32 k = 0;
    while (0 != strncasecmp(column[k].name, "NULL", MAX_NAMESIZE))
    {
        if (k)
            query_string << ",";
        query_string << column[k].name;
        k++;
    }
    std::string lowTable(tableName);
    query_string << " FROM " << TO_LOWER_STR(lowTable);
    if (NULL != where)
        query_string << " WHERE " << where;
    if (NULL != extraOpt)
        query_string << " " << extraOpt;
#if defined(_SQL_DEBUG_ZYX)
    //XDBG("[MYSQL SELECT],%s", query_string.str().c_str());
#endif
    MYSQL_RES *res;
    MYSQL_ROW row;

    /* send SQL query */
    if (mysql_real_query(mysql, query_string.str().c_str(), query_string.str().size()))
    {
        XERR("[MYSQL]query error! %s", query_string.str().c_str());
        return DBErrReturn;
    }
    uint32 count = 0, place = 0;
    res = mysql_store_result(mysql);
    int num_fields = mysql_num_fields(res);

    //*data = new unsigned char[MAX_DBDATA_SIZE];
    //memset(*data, 0x00, MAX_DBDATA_SIZE);
    unsigned long *lengths;
    while ((row = mysql_fetch_row(res)) != NULL)
    {
        lengths = mysql_fetch_lengths(res);
        for (int i = 0; i < num_fields; i++)
        {
            place = count;
            count += column[i].size;
#ifdef _SQL_DEBUG
            XDBG("[MYSQL]mysql_fetch_row total:%lu, lengths[%u]:%lu", count, i, place);
#endif
            if (MAX_DBDATA_SIZE <= count)
            {
                XERR("[MYSQL],%s鏌ヨ鏁版嵁澶у皬瓒呰繃闄愬埗!", tableName);
                //SAFE_DELETE_VEC(*data);
                mysql_free_result(res);
                return DBErrReturn;
            }
            switch (column[i].type)
            {
            case DBDATA_TYPE_CHAR:
            {
                if ((unsigned char *)row[i]) {
                    bcopy((unsigned char *)row[i], (unsigned char *)(&(data)[place]), column[i].size);
                }
            }
            break;
            case DBDATA_TYPE_UInt8:
            {
                *(uint8 *)(&(data)[place]) = atoi(row[i]);
            }
            break;
            case DBDATA_TYPE_INT:
            {
                *(int *)(&(data)[place]) = atoi(row[i]);
            }
            break;
            case DBDATA_TYPE_UInt16:
            {
                *(uint16 *)(&(data)[place]) = atoi(row[i]);
            }
            break;
            case DBDATA_TYPE_UInt32:
            {
                *(uint32 *)(&(data)[place]) = atoi(row[i]);
            }
            break;
            case DBDATA_TYPE_UInt64:
            {
                *(uint64 *)(&(data)[place]) = atoll(row[i]);
            }
            break;
            case DBDATA_TYPE_BIN:
            {
                if ((unsigned char *)row[i]) {
                    bcopy((unsigned char *)row[i], (unsigned char *)(&(data)[place]), column[i].size);
                }
            }
            break;
            case DBDATA_TYPE_BIN2:
            {
                count += lengths[i];
                if (MAX_DBDATA_SIZE <= count)
                {
                    XERR("[MYSQL],%s鏌ヨ鏁版嵁澶у皬瓒呰繃闄愬埗!", tableName);
                    //SAFE_DELETE_VEC(*data);
                    mysql_free_result(res);
                    return DBErrReturn;
                }
                if (lengths[i])
                    bcopy((unsigned char *)row[i], (unsigned char *)(&(data)[place]), lengths[i]);
                else
                {
                    *(uint32 *)(&(data)[place]) = 0;
                    count += sizeof(uint32);
                }
            }
            break;
            default:
                break;
            }
        }
    }
    int ret = (int)mysql_num_rows(res);

    mysql_free_result(res);
    return ret;
}

uint64 DBConn::checkExist(const char *table, const char *where)
{
    if (NULL == table) return DBErrReturn;

    std::string lowTable(table);
    std::stringstream query_string;
    query_string.str("");
    query_string << "SELECT 1 FROM " << TO_LOWER_STR(lowTable);
    if (NULL != where)
        query_string << " WHERE " << where;
    /* send SQL query */  
    if (mysql_real_query(mysql, query_string.str().c_str(), query_string.str().size()))   
    {   
        XERR("[MYSQL],%s", query_string.str().c_str());
        return DBErrReturn;
    }   
    MYSQL_RES *res=mysql_store_result(mysql);;
    uint32 num=mysql_num_rows(res);
    mysql_free_result(res);
    return num;
}

uint64 DBConn::getNum(const char *table,const char *where)
{
    if (NULL == table) return DBErrReturn;

    std::string lowTable(table);
    std::stringstream query_string;
    query_string.str("");
    query_string << "SELECT count(*) FROM " << TO_LOWER_STR(lowTable);
    if (NULL != where)
        query_string << " WHERE " << where;
#if defined(_SQL_DEBUG_ZYX)
    XDBG("[MYSQL GETNUM],%s", query_string.str().c_str());
#endif
    /* send SQL query */  
    if (mysql_real_query(mysql, query_string.str().c_str(), query_string.str().size()))   
    {   
        XERR("[MYSQL],%s", query_string.str().c_str());
        return DBErrReturn;
    }   
    uint64 num = DBErrReturn;
    MYSQL_RES *res=mysql_store_result(mysql);
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row != NULL && mysql_num_fields(res) > 0)
        num = atoll(row[0]);
    mysql_free_result(res);
    return num;
}

DBConnPool::~DBConnPool()
{
    pool_iter it=pool.begin(), tmp=it;
    for (; it!=pool.end(); )
    {
        tmp = it++;
        mysql_close(tmp->second->mysql);
        do { delete tmp->second; } while (false);
        pool.erase(tmp);
    }

#ifndef _WIN32
    mysql_server_end(); 
#endif
}

bool DBConnPool::init()
{
#ifndef _WIN32
    if (mysql_server_init(0,NULL,NULL))
        return false;
#endif
    XDBG("[DBConnPool],DBPool init max:%u", _max);
    for (uint32 i=1; i<=_max; i++)
    {
        DBConn *conn = new DBConn();
        if (!conn)
        {
            XERR("[DBConn],create obj error !!! ");
            return false;
        }
        if (!conn->init(_server.c_str(), _user.c_str(), _password.c_str(), _database.c_str())) 
        {
            XERR("[DBConn],init error [_server:%s,_user:%s,_password:%s,_database:%s] !!!", 
                 _server.c_str(), _user.c_str(), _password.c_str(), _database.c_str());

            return false;
        }

        pool.insert(std::make_pair(i, conn));
        XDBG("[DBConnPool],DBPool init %u", i);
    }
    ids = new UniqueIDManager<DBConnID>(1, _max+1);
    return true;
}

bool DBConnPool::init(const char *pHost, int iPort, const char *pDBName, const char *pUser, const char *pPassWd, int iPoolCount) {
    _server = pHost;
    _port = iPort;
    _user = pUser;
    _password = pPassWd;
    _database = pDBName;

    _max = iPoolCount;

    return init();
}

void DBConnPool::freeID()
{
    if (ids)
        ids->clear();
}  

DBConnID DBConnPool::getDBConn()
{
    if (NULL == ids) return DBErrConnID;
    
    DBConnID id;
    ids->getUniqueID(id);
#ifdef _SQL_DEBUG
    XDBG("[DBConnPool],鑾峰緱杩炴帴 %u", id);
#endif
    return id;
}

void DBConnPool::putDBConn(DBConnID con)
{
    if (NULL == ids) return;
#ifdef _SQL_DEBUG
    XDBG("[DBConnPool],閲婃斁杩炴帴");
#endif
    ids->putUniqueID(con);
}
