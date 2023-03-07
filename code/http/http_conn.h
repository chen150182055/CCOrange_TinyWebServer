#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"

//http_conn类
class http_conn {
public:     //公有成员
    static const int FILENAME_LEN = 200;        //表示文件名的最大长度
    static const int READ_BUFFER_SIZE = 2048;   //表示读缓冲区的大小
    static const int WRITE_BUFFER_SIZE = 1024;  //表示写缓冲区的大小

    //定义了HTTP请求的方法，包括GET、POST、HEAD、PUT、DELETE、TRACE、OPTIONS、CONNECT和PATH
    enum METHOD {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };

    //定义了解析HTTP请求的状态，分别表示正在分析请求行、请求头、请求体
    enum CHECK_STATE {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };

    //定义了HTTP请求的返回状态码
    enum HTTP_CODE {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };

    //定义了解析行的状态
    enum LINE_STATUS {
        LINE_OK = 0,    //解析成功
        LINE_BAD,       //行出现语法错误
        LINE_OPEN       //行数据尚不完整
    };

public:
    http_conn() {}  //构造函数

    ~http_conn() {} //析构函数

public:     //公有成员
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, string passwd, string sqlname);

    void close_conn(bool real_close = true);

    void process();

    bool read_once();

    bool write();

    sockaddr_in *get_address() {
        return &m_address;
    }

    void initmysql_result(connection_pool *connPool);

    int timer_flag;
    int improv;


private:
    void init();

    HTTP_CODE process_read();

    bool process_write(HTTP_CODE ret);

    HTTP_CODE parse_request_line(char *text);

    HTTP_CODE parse_headers(char *text);

    HTTP_CODE parse_content(char *text);

    HTTP_CODE do_request();

    char *get_line() { return m_read_buf + m_start_line; };

    LINE_STATUS parse_line();

    void unmap();

    bool add_response(const char *format, ...);

    bool add_content(const char *content);

    bool add_status_line(int status, const char *title);

    bool add_headers(int content_length);

    bool add_content_type();

    bool add_content_length(int content_length);

    bool add_linger();

    bool add_blank_line();

public:
    static int m_epollfd;       //表示当前类所对应的 epollfd 文件描述符
    static int m_user_count;    //表示当前连接的客户数量
    MYSQL *mysql;               //表示 MySQL 数据库连接句柄
    int m_state;                //表示当前连接的状态，0 表示读，1 表示写

private:    //私有成员
    int m_sockfd;           //表示连接的套接字描述符
    sockaddr_in m_address;  //表示连接的客户端地址
    char m_read_buf[READ_BUFFER_SIZE];  //表示读缓冲区
    int m_read_idx;         //表示当前读缓冲区中数据的末尾位置
    int m_checked_idx;      //表示当前正在分析的字符在读缓冲区中的位置
    int m_start_line;       //表示当前正在分析的行的起始位置
    char m_write_buf[WRITE_BUFFER_SIZE];//表示写缓冲区
    int m_write_idx;        //表示当前写缓冲区中待发送的字节数
    CHECK_STATE m_check_state;          //表示当前的解析状态
    METHOD m_method;        //表示当前请求的方法类型
    char m_real_file[FILENAME_LEN];     //表示请求的文件在服务器上的真实路径
    char *m_url;            //表示请求的 URL
    char *m_version;        //表示 HTTP 版本
    char *m_host;           //表示请求的主机地址
    int m_content_length;   //表示请求消息体的长度
    bool m_linger;          //表示是否保持连接
    char *m_file_address;   //表示请求的文件在内存中的起始位置
    struct stat m_file_stat;//表示请求文件的状态
    struct iovec m_iv[2];   //表示写缓冲区中待发送的数据
    int m_iv_count;         //表示写缓冲区中待发送的数据数量
    int cgi;                //是否启用的POST
    char *m_string;         //存储请求头数据
    int bytes_to_send;      //表示待发送的字节数
    int bytes_have_send;    //表示已发送的字节数
    char *doc_root;         //表示服务器的根目录

    map <string, string> m_users;       //表示用户列表
    int m_TRIGMode;         //表示触发模式
    int m_close_log;        //表示是否关闭日志

    char sql_user[100];     //表示数据库用户名
    char sql_passwd[100];   //表示数据库密码
    char sql_name[100];     //表示数据库名
};

#endif
