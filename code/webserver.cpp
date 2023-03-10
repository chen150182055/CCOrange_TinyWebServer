#include "webserver.h"

/**
 * @brief 构造函数
 */
WebServer::WebServer() {

    users = new http_conn[MAX_FD];  //http_conn类对象

    char server_path[200];      //root文件夹路径
    chdir("../");               //改变当前工作路径
    getcwd(server_path, 200);   //获取当前工作路径,将值存放在server_path中,200为空间大小
    char root[20] = "/staticResources";
    //计算server_path和root的长度和
    m_root = (char *) malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);//将server_path复制到m_root
    strcat(m_root, root);       //将root复制到m_root

    users_timer = new client_data[MAX_FD];  //定时器
}

/**
 * @brief 析构函数
 */
WebServer::~WebServer() {
    close(m_epollfd);   //关闭 epoll 文件描述符
    close(m_listenfd);  //停止监听套接字
    close(m_pipefd[1]); //关闭管道文件描述符
    close(m_pipefd[0]); //关闭管道文件描述符
    delete[] users;     //释放 http_conn 类对象数组
    delete[] users_timer;//释放定时器
    delete m_pool;      //释放线程池
}

/**
 * @brief 初始化函数，用来初始化 Web 服务器的一些配置参数和数据库连接池等
 * @param port 端口号
 * @param user 用户名
 * @param passWord 密码
 * @param databaseName 数据库名称
 * @param log_write 日志写入
 * @param opt_linger 选项延迟
 * @param trigmode 触发模式
 * @param sql_num 数据库数量
 * @param thread_num 线程数量
 * @param close_log 关闭日志
 * @param actor_model actor模型
 */
void WebServer::init(int port, string user, string passWord, string databaseName, int log_write,
                     int opt_linger, int trigmode, int sql_num, int thread_num, int close_log,
                     int actor_model) {
    m_port = port;                  //初始化端口号
    m_user = user;                  //初始化用户
    m_passWord = passWord;          //初始化密码
    m_databaseName = databaseName;  //初始化数据库名称
    m_sql_num = sql_num;            //初始化数据库数量
    m_thread_num = thread_num;      //初始化线程数量
    m_log_write = log_write;        //初始化日志写入方式
    m_OPT_LINGER = opt_linger;      //初始化选项延迟
    m_TRIGMode = trigmode;          //初始化触发模式
    m_close_log = close_log;        //初始化关闭日志
    m_actormodel = actor_model;     //初始化事件模型
}

/**
 * @brief 将服务器的触发模式从整数值转换为实际的LT和ET模式
 * 边缘触发（ET）和水平触发（LT）。
 * 边缘触发模式会在数据有变化时才会触发事件，
 * 而水平触发模式则会在数据仍然可读或可写时不断触发事件
 */
void WebServer::trig_mode() {   //触发模式
    //如果 m_TRIGMode 的值为0，则表示使用LT+LT模式
    if (0 == m_TRIGMode) {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 0;
    }
        //如果 m_TRIGMode 的值为1，则表示使用LT+ET模式
    else if (1 == m_TRIGMode) {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
    }
        //如果 m_TRIGMode 的值为2，则表示使用ET+LT模式
    else if (2 == m_TRIGMode) {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 0;
    }
        //如果 m_TRIGMode 的值为3，则表示使用ET+ET模式
    else if (3 == m_TRIGMode) {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
    }
}

/**
 * @brief 打开日志文件，用来记录 Web 服务器的运行日志
 */
void WebServer::log_write() {   //日志
    if (0 == m_close_log) {
        //初始化日志
        if (1 == m_log_write)
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 800);
        else
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 0);
    }
}

/**
 * @brief 创建数据库连接池，用来处理数据库查询请求
 */
void WebServer::sql_pool() {    //数据库
    //初始化数据库连接池
    m_connPool = connection_pool::GetInstance();
    m_connPool->init("localhost", m_user, m_passWord, m_databaseName, 3306, m_sql_num, m_close_log);

    //初始化数据库读取表
    users->initmysql_result(m_connPool);
}

/**
 * @brief 创建线程池，用来处理客户端请求
 */
void WebServer::thread_pool() {
    //线程池
    m_pool = new threadpool<http_conn>(m_actormodel, m_connPool, m_thread_num);
}

/**
 * @brief 创建并监听套接字，等待客户端连接请求
 */
void WebServer::eventListen() {     //监听
    //网络编程基础步骤

    //1.创建套接字
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);   //ipv4和字节流，SOCK_STREAM      DRAGM

    assert(m_listenfd >= 0);

    //2.优雅关闭连接
    if (0 == m_OPT_LINGER) {
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    } else if (1 == m_OPT_LINGER) {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));  //可重用TIME_WAIT状态的TCP连接

    //3.绑定地址，将套接字绑定到地址和端口上
    ret = bind(m_listenfd, (struct sockaddr *) &address, sizeof(address));  //命名socket，命名到服务器本机的所有网卡的9006端口
    assert(ret >= 0);

    //4.监听套接字
    ret = listen(m_listenfd, 5);    //创建监听队列，监听，accecpt
    assert(ret >= 0);

    //5.初始化定时器
    utils.init(TIMESLOT);

    //6.epoll创建内核事件表 epoll poll select epoll_create 内核事件表 epoll_wait epoll_ctl
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);    //size 5已经失效
    assert(m_epollfd != -1);

    //7.添加监听套接字到事件表，这里将第三个参数设置为 false，表示使用 LT 模式，将第四个参数设置为 m_LISTENTrigmode，表示触发模式
    utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
    //8.将 HTTP 连接的 epoll 事件表设置为 m_epollfd
    http_conn::m_epollfd = m_epollfd;

    //9.建立双向管道来发送信号，将可读可写事件与信号事件统一事件源
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);    //1写0读，将两端都非阻塞LT，然后epollfd监听0读端
    assert(ret != -1);
    utils.setnonblocking(m_pipefd[1]);
    utils.addfd(m_epollfd, m_pipefd[0], false, 0);

    utils.addsig(SIGPIPE, SIG_IGN);
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false);

    alarm(TIMESLOT);

    //工具类,信号和描述符基础操作
    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}

/**
 * @brief 进入事件循环，处理客户端连接请求和定时器事件
 */
void WebServer::eventLoop() {
    bool timeout = false;
    bool stop_server = false;

    while (!stop_server) {
        //使用 epoll_wait() 函数来等待事件的发生
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR) {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        //遍历所有就绪事件，处理事件
        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;

            //1.表示有新的客户端连接请求，调用 dealclinetdata() 函数来处理连接请求
            if (sockfd == m_listenfd) {
                bool flag = dealclinetdata();
                if (false == flag)
                    continue;
            }
            //2.表示客户端连接已经断开，移除对应的定时器，并清理相应的资源
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                //服务器端关闭连接，移除对应的定时器
                util_timer *timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            }
            //3.表示收到了一个信号，调用 dealwithsignal() 函数来处理信号
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN)) {
                bool flag = dealwithsignal(timeout, stop_server);
                if (false == flag)
                    LOG_ERROR("%s", "dealclientdata failure");
            }
            //4.表示客户端有数据到来，调用 dealwithread() 函数来处理客户端请求
            else if (events[i].events & EPOLLIN) {
                dealwithread(sockfd);
            }
            //5.表示服务器端可以向客户端发送数据，调用 dealwithwrite() 函数来发送数据
            else if (events[i].events & EPOLLOUT) {
                dealwithwrite(sockfd);
            }
        }
        if (timeout) {
            //当 timeout 标志位为 true 时，说明定时器事件已经触发，调用 timer_handler() 函数来处理定时器事件
            utils.timer_handler();

            LOG_INFO("%s", "timer tick");
            //将 timeout 标志位设置为 false，表示定时器事件已经处理完毕
            timeout = false;
        }
    }
}

/**
 * @brief 添加定时器，用来处理客户端连接超时事件
 * @param connfd
 * @param client_address
 */
void WebServer::timer(int connfd, struct sockaddr_in client_address) {
    users[connfd].init(connfd, client_address, m_root, m_CONNTrigmode, m_close_log, m_user, m_passWord, m_databaseName);

    //初始化client_data数据
    //创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    util_timer *timer = new util_timer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;
    utils.m_timer_lst.add_timer(timer);
}

/**
 * @brief 调整定时器，用来更新定时器的超时时间
 * @param timer
 */
void WebServer::adjust_timer(util_timer *timer) {
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    utils.m_timer_lst.adjust_timer(timer);

    LOG_INFO("%s", "adjust timer once");
}

/**
 * @brief 处理定时器事件，用来关闭超时的客户端连接
 * @param timer
 * @param sockfd
 */
void WebServer::deal_timer(util_timer *timer, int sockfd) {
    timer->cb_func(&users_timer[sockfd]);
    if (timer) {
        utils.m_timer_lst.del_timer(timer);
    }

    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

/**
 * @brief 处理客户端数据，包括解析 HTTP 请求和生成 HTTP 响应
 * @return
 */
bool WebServer::dealclinetdata() {
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    if (0 == m_LISTENTrigmode) {    //LT模式
        //获取到的新客户连接fd
        int connfd = accept(m_listenfd, (struct sockaddr *) &client_address, &client_addrlength);
        if (connfd < 0) {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        if (http_conn::m_user_count >= MAX_FD) {
            utils.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        //将connfd添加到epollfd中
        timer(connfd, client_address);
    } else {    //监听socket是ET模式
        while (1) {
            int connfd = accept(m_listenfd, (struct sockaddr *) &client_address, &client_addrlength);
            if (connfd < 0) {
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            if (http_conn::m_user_count >= MAX_FD) {
                utils.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            timer(connfd, client_address);
        }
        return false;
    }
    return true;
}

/**
 * @brief 处理信号事件，包括定时器信号和终止信号
 * @param timeout
 * @param stop_server
 * @return
 */
bool WebServer::dealwithsignal(bool &timeout, bool &stop_server) {
    int ret = 0;
    int sig;
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (ret == -1) {
        return false;
    } else if (ret == 0) {
        return false;
    } else {
        for (int i = 0; i < ret; ++i) {
            switch (signals[i]) {
                case SIGALRM: {
                    timeout = true;
                    break;
                }
                case SIGTERM: {
                    stop_server = true;
                    break;
                }
            }
        }
    }
    return true;
}

/**
 * @brief 处理读事件，用来读取客户端发送的数据
 * @param sockfd
 */
void WebServer::dealwithread(int sockfd) {
    util_timer *timer = users_timer[sockfd].timer;

    //reactor
    if (1 == m_actormodel) {
        if (timer) {
            adjust_timer(timer);
        }

        //若监测到读事件，将该事件放入请求队列    user --> http_conn users[10000];
        m_pool->append(users + sockfd, 0);  // sockfd 从内核空间读到用户空间，用户肯定要有内存 sockfd http_conn user[sockfd]

        while (true) {
            if (1 == users[sockfd].improv) {
                if (1 == users[sockfd].timer_flag) {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    } else {
        //proactor
        if (users[sockfd].read_once()) {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            //若监测到读事件，将该事件放入请求队列
            m_pool->append_p(users + sockfd);

            if (timer) {
                adjust_timer(timer);
            }
        } else {
            deal_timer(timer, sockfd);
        }
    }
}

/**
 * @brief 处理写事件，用来向客户端发送数据
 * @param sockfd
 */
void WebServer::dealwithwrite(int sockfd) {
    util_timer *timer = users_timer[sockfd].timer;
    //reactor
    if (1 == m_actormodel) {
        if (timer) {
            adjust_timer(timer);
        }

        m_pool->append(users + sockfd, 1);

        while (true) {
            if (1 == users[sockfd].improv) {
                if (1 == users[sockfd].timer_flag) {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    } else {
        //proactor
        if (users[sockfd].write()) {
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            if (timer) {
                adjust_timer(timer);
            }
        } else {
            deal_timer(timer, sockfd);
        }
    }
}
