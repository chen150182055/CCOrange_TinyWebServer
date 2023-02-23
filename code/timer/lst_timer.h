#ifndef LST_TIMER
#define LST_TIMER

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

#include <time.h>
#include "../log/log.h"

//util_timer类声明
class util_timer;

struct client_data {
    sockaddr_in address;
    int sockfd;
    util_timer *timer;
};

//util_timer类
class util_timer {
public:     //公有成员
    util_timer() : prev(NULL), next(NULL) {}    //构造函数

public:     //公有成员
    time_t expire;

    void (*cb_func)(client_data *);

    client_data *user_data;
    util_timer *prev;
    util_timer *next;
};

//定时器链表类，用于管理服务器所有连接对应的定时器
class sort_timer_lst {
public:     //公有成员
    sort_timer_lst();   //构造函数声明

    ~sort_timer_lst();  //析构函数声明

    void add_timer(util_timer *timer);

    void adjust_timer(util_timer *timer);

    void del_timer(util_timer *timer);

    void tick();

private:    //私有成员
    void add_timer(util_timer *timer, util_timer *lst_head);

    util_timer *head;   //链表的头指针
    util_timer *tail;   //链表的尾指针
};

//Utils类
class Utils {
public:     //公有成员
    Utils() {}  //构造函数声明

    ~Utils() {} //析构函数声明

    void init(int timeslot);

    //对文件描述符设置非阻塞
    int setnonblocking(int fd);

    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    //信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;       //管道文件描述符指针
    sort_timer_lst m_timer_lst; //定时器链表
    static int u_epollfd;       //表示 Utils 类中的所有对象共享同一个 epoll 文件描述符
    int m_TIMESLOT;             //表示定时器的最小时间间隔
};

void cb_func(client_data *user_data);   //回调函数

#endif
