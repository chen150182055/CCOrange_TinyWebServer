#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

//connection_pool类
class connection_pool {
public:     //公有成员
    MYSQL *GetConnection();                 //获取数据库连接
    bool ReleaseConnection(MYSQL *conn);    //释放连接
    int GetFreeConn();                      //获取连接
    void DestroyPool();                     //销毁所有连接

    static connection_pool *GetInstance();  //单例模式

    void init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log);

private:    //私有成员
    connection_pool();  //构造函数

    ~connection_pool(); //析构函数

    int m_MaxConn;  //最大连接数
    int m_CurConn;  //当前已使用的连接数
    int m_FreeConn; //当前空闲的连接数
    locker lock;
    list<MYSQL *> connList; //连接池
    sem reserve;

public:     //公有成员
    string m_url;             //主机地址
    string m_Port;         //数据库端口号
    string m_User;         //登陆数据库用户名
    string m_PassWord;     //登陆数据库密码
    string m_DatabaseName; //使用数据库名
    int m_close_log;    //日志开关
};

//connectionRAII类
class connectionRAII {
public:     //公有成员
    connectionRAII(MYSQL **con, connection_pool *connPool); //构造函数

    ~connectionRAII();                                      //析构函数

private:    //私有成员
    MYSQL *conRAII;
    connection_pool *poolRAII;
};

#endif
