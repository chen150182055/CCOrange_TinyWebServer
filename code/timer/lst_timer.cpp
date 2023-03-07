#include "lst_timer.h"
#include "../http/http_conn.h"

/**
 * @brief
 */
sort_timer_lst::sort_timer_lst() {
    head = NULL;
    tail = NULL;
}

/**
 * @brief
 */
sort_timer_lst::~sort_timer_lst() {
    util_timer *tmp = head;
    while (tmp) {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

/**
 * @brief 将一个计时器添加到已排序的计时器链表中，并保持链表的顺序
 * @param timer 指向计时器对象的指针，表示要添加的计时器
 */
void sort_timer_lst::add_timer(util_timer *timer) {
    if (!timer) {
        return;
    }
    if (!head) {
        head = tail = timer;
        return;
    }
    if (timer->expire < head->expire) {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    add_timer(timer, head);
}

/**
 * @brief 调整指定定时器的位置，这个函数被用于定时器已经发生超时，
 * 但是在处理定时器超时之前又被修改了超时时间的情况，
 * 此时需要将定时器从链表中删除，并重新插入链表，
 * 以保证链表中的所有定时器按照超时时间从小到大排序
 * @param timer
 */
void sort_timer_lst::adjust_timer(util_timer *timer) {
    if (!timer) {
        return;
    }
    util_timer *tmp = timer->next;
    if (!tmp || (timer->expire < tmp->expire)) {
        return;
    }
    if (timer == head) {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    } else {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

/**
 * @brief 将一个定时器从定时器链表中删除
 * @param timer
 */
void sort_timer_lst::del_timer(util_timer *timer) {
    if (!timer) {
        return;
    }
    if ((timer == head) && (timer == tail)) {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    if (timer == head) {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    if (timer == tail) {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

/**
 * @brief 处理定时任务，它会被定时器处理线程以固定时间间隔调用，
 * 用于检查定时器链表中所有定时器是否超时
 */
void sort_timer_lst::tick() {
    if (!head) {
        return;
    }

    time_t cur = time(NULL);
    util_timer *tmp = head;
    while (tmp) {
        if (cur < tmp->expire) {
            break;
        }
        tmp->cb_func(tmp->user_data);
        head = tmp->next;
        if (head) {
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}

/**
 * @brief 将一个定时器插入到定时器链表中，保证链表中所有定时器按照其过期时间的先后顺序排列
 * @param timer 指向要添加的定时器的指针
 * @param lst_head 指向定时器链表头结点的指针
 */
void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head) {
    util_timer *prev = lst_head;
    util_timer *tmp = prev->next;
    while (tmp) {
        if (timer->expire < tmp->expire) {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    if (!tmp) {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

/**
 * @brief 初始化定时器，并设置最小超时单位
 * @param timeslot
 */
void Utils::init(int timeslot) {
    m_TIMESLOT = timeslot;
}

/**
 * @brief 将文件描述符设置为非阻塞模式
 * @param fd
 * @return
 */
int Utils::setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

/**
 * @brief 将文件描述符加入到epoll事件监听中，并设置EPOLLONESHOT模式
 * @param epollfd
 * @param fd
 * @param one_shot
 * @param TRIGMode
 */
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode) {
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}


/**
 * @brief 信号处理函数
 * @param sig
 */
void Utils::sig_handler(int sig) {
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *) &msg, 1, 0);
    errno = save_errno;
}


/**
 * @brief 注册信号处理函数
 * @param sig
 * @param handler
 * @param restart
 */
void Utils::addsig(int sig, void(handler)(int), bool restart) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}


/**
 * @brief 定时处理函数，重新定时以不断触发SIGALRM信号
 */
void Utils::timer_handler() {
    m_timer_lst.tick();
    alarm(m_TIMESLOT);
}

/**
 * @brief 显示错误信息
 * @param connfd
 * @param info
 */
void Utils::show_error(int connfd, const char *info) {
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;

/**
 * @brief 回调函数
 * @param user_data
 */
void cb_func(client_data *user_data) {
    //通过 epoll_ctl 函数将该定时器所对应的文件描述符从 epoll 实例中删除
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    //通过 assert 断言确保该定时器所关联的客户端数据不为空
    assert(user_data);
    //接着关闭客户端对应的 socket 连接
    close(user_data->sockfd);
    //将 http_conn::m_user_count 计数器减 1，表示当前连接的客户端数量减少了一个
    http_conn::m_user_count--;
}
