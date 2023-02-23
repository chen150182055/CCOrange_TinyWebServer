#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

using namespace std;

/**
 * @brief 构造函数，初始化连接池
 */
connection_pool::connection_pool()
{
    m_CurConn = 0;
    m_FreeConn = 0;
}

/**
 * @brief 返回一个连接池对象的指针
 * 确保了整个应用程序共享同一个连接池
 * 这对于需要频繁使用数据库连接的应用程序来说是很有用的，
 * 因为它避免了每次需要数据库操作时都重新创建一个连接池的开销
 * @return
 */
connection_pool *connection_pool::GetInstance()
{
    //创建一个静态实例，是该类的一个实例
    //static关键字的作用是使该对象只在函数首次调用时创建一次，之后每次调用函数时都将返回同一个对象
    static connection_pool connPool;
    return &connPool;
}

/**
 * @brief 构造初始化
 * @param url
 * @param User
 * @param PassWord
 * @param DBName
 * @param Port
 * @param MaxConn
 * @param close_log
 */
void connection_pool::init(string url, string User, string PassWord, string DBName, int Port, int MaxConn, int close_log)
{
    m_url = url;    //主机号
    m_Port = Port;  //端口号
    m_User = User;  //用户名
    m_PassWord = PassWord;  //密码
    m_DatabaseName = DBName;//数据库名
    m_close_log = close_log;//日志开关

    for (int i = 0; i < MaxConn; i++)
    {
        MYSQL *con = NULL;
        con = mysql_init(con);  //初始化mysql连接

        if (con == NULL)
        {
            LOG_ERROR("MySQL Error");
            exit(1);
        }
        con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);  //建立一个到mysql的连接

        if (con == NULL)
        {
            LOG_ERROR("MySQL Error");
            exit(1);
        }
        connList.push_back(con);
        ++m_FreeConn;
    }

    reserve = sem(m_FreeConn);

    m_MaxConn = m_FreeConn;
}


/**
 * @brief 当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
 * @return
 */
MYSQL *connection_pool::GetConnection()
{
    MYSQL *con = NULL;

    if (0 == connList.size())
        return NULL;

    reserve.wait();

    lock.lock();

    con = connList.front();
    connList.pop_front();

    --m_FreeConn;
    ++m_CurConn;

    lock.unlock();
    return con;
}

/**
 * @brief 释放当前使用的连接
 * @param con
 * @return
 */
bool connection_pool::ReleaseConnection(MYSQL *con)
{
    if (NULL == con)
        return false;

    lock.lock();

    connList.push_back(con);
    ++m_FreeConn;
    --m_CurConn;

    lock.unlock();

    reserve.post();
    return true;
}

/**
 * @brief 销毁数据库连接池
 */
void connection_pool::DestroyPool()
{

    lock.lock();
    if (connList.size() > 0)
    {
        list<MYSQL *>::iterator it;
        for (it = connList.begin(); it != connList.end(); ++it)
        {
            MYSQL *con = *it;
            mysql_close(con); //关闭mysql连接
        }
        m_CurConn = 0;
        m_FreeConn = 0;
        connList.clear();
    }

    lock.unlock();
}

/**
 * @brief 当前空闲的连接数
 * @return
 */
int connection_pool::GetFreeConn()
{
    return this->m_FreeConn;
}

/**
 * @brief 析构函数
 */
connection_pool::~connection_pool()
{
    DestroyPool();
}

/**
 * @brief 构造函数
 * 资源获取即初始化
 * 这种模式下，资源在对象初始化时被获取，并在对象销毁时自动释放，从而确保资源的正确管理和释放
 * connectionRAII类用来管理数据库连接的生命周期，
 * 确保连接在使用完毕后能够被正确释放，
 * 避免内存泄漏和数据库连接池中连接的过度占用
 * @param SQL
 * @param connPool
 */
connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool){
    //获取一个可用的数据库连接，并将该连接存储在传入的SQL指针中
    *SQL = connPool->GetConnection();

    conRAII = *SQL;
    poolRAII = connPool;
}

/**
 * @brief 析构函数
 */
connectionRAII::~connectionRAII(){
    poolRAII->ReleaseConnection(conRAII);
}