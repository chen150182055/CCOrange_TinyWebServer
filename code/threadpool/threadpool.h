#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

//类模板的模板参数为 T，表示任务类型
template<typename T>
class threadpool {
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool(int actor_model, connection_pool *connPool, int thread_number = 8, int max_request = 10000);

    ~threadpool();

    bool append(T *request, int state);

    bool append_p(T *request);

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);

    void run();

private:
    int m_thread_number;        //线程池中的线程数
    int m_max_requests;         //请求队列中允许的最大请求数
    pthread_t *m_threads;       //描述线程池的数组，其大小为m_thread_number
    std::list<T *> m_workqueue; //请求队列
    locker m_queuelocker;       //保护请求队列的互斥锁
    sem m_queuestat;            //是否有任务需要处理
    connection_pool *m_connPool;//数据库
    int m_actor_model;          //模型切换
};


/**
 * @brief 构造函数，用于初始化线程池
 * @tparam T
 * @param actor_model 模型切换
 * @param connPool 数据库连接池
 * @param thread_number 线程数量
 * @param max_requests 请求队列中允许的最大请求数
 */
template<typename T>
threadpool<T>::threadpool(int actor_model, connection_pool *connPool, int thread_number, int max_requests)
        : m_actor_model(actor_model), m_thread_number(thread_number), m_max_requests(max_requests), m_threads(NULL),
          m_connPool(connPool) {
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();

    //使用new动态分配了一个大小为thread_number的pthread_t数组，用于存放线程ID
    m_threads = new pthread_t[m_thread_number];

    if (!m_threads)
        throw std::exception();

    //循环创建thread_number个工作线程
    for (int i = 0; i < thread_number; ++i) {

        //具体的，首先使用pthread_create函数创建一个线程，将该线程的ID存储在m_threads数组中
        if (pthread_create(m_threads + i, NULL, worker, this) != 0) {
            //如果创建线程失败，将m_threads数组释放，然后抛出异常
            delete[] m_threads;
            throw std::exception();
        }

        //使用pthread_detach函数将该线程设置为分离状态，这样的话，当线程执行结束后，系统会自动回收该线程所占用的资源
        if (pthread_detach(m_threads[i])) {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

/**
 * @brief 析构函数
 * @tparam T
 */
template<typename T>
threadpool<T>::~threadpool() {
    //释放线程数组所占用的内存
    delete[] m_threads;
}

/**
 * @brief 向线程池的工作队列中添加任务
 * @tparam T
 * @param request
 * @param state
 * @return
 */
template<typename T>
bool threadpool<T>::append(T *request, int state) {
    //先对工作队列进行加锁
    m_queuelocker.lock();
    //判断队列中任务数量是否已达到最大限制
    if (m_workqueue.size() >= m_max_requests) {
        //则解锁并返回 false
        m_queuelocker.unlock();
        return false;
    }
    //将传入的请求 request 加入队列尾部，设置请求的状态为 state
    request->m_state = state;
    m_workqueue.push_back(request);
    //解锁并发送信号通知工作线程有新任务需要处理
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

/**
 * @brief 将请求添加到工作队列中
 * @tparam T
 * @param request
 * @return
 */
template<typename T>
bool threadpool<T>::append_p(T *request) {
    //对请求队列上锁
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests) {
        //解锁并返回false
        m_queuelocker.unlock();
        return false;
    }
    //将请求加入队列，解锁并将信号量m_queuestat的值加1，表示有新任务需要处理
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

/**
 * @brief 工作线程运行的函数，它不断从工作队列中取出任务并执行之
 * @tparam T
 * @param arg
 * @return
 */
template<typename T>
void *threadpool<T>::worker(void *arg) {
    //将arg转换成线程池指针pool
    threadpool *pool = (threadpool *) arg;
    //调用pool的run函数
    pool->run();
    return pool;
}

/**
 * @brief 主要运行逻辑,不断从工作队列中取出任务并执行之
 * @tparam T
 */
template<typename T>
void threadpool<T>::run() {
    while (true) {
        //消费者
        //1.通过信号量 m_queuestat 来阻塞线程，直到有任务需要处理
        m_queuestat.wait(); //sem
        //2.获取工作队列的互斥锁 m_queuelocker，如果工作队列为空，立即释放互斥锁并继续等待信号量
        m_queuelocker.lock();//lock

        if (m_workqueue.empty()) {
            m_queuelocker.unlock();
            continue;
        }

        //3.从工作队列中取出一个任务 request 并从队列中删除
        T *request = m_workqueue.front(); //返回容器中的第一个元素
        m_workqueue.pop_front();          //删除容器中的第一个元素

        //4.释放工作队列的互斥锁
        m_queuelocker.unlock();
        if (!request)
            continue;
        //5.根据 actor_model 的值来确定任务的处理方式
        if (1 == m_actor_model) {   //reactor
            //如果是 1，则表示使用 reactor 模式，需要根据任务的状态来确定是读取数据还是写入数据
            if (0 == request->m_state) {    //读事件
                if (request->read_once()) {
                    //如果是读取数据，则先进行一次读取，如果读取成功则调用 request->process() 处理请求
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, m_connPool);
                    request->process();
                } else {
                    //否则将 timer_flag 置为 1，表示需要定时关闭请求
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            } else {
                if (request->write()) {     //写事件
                    //如果是写入数据，则进行一次写操作,如果写入成功则将 request->improv 置为 1，表示请求已经被处理
                    request->improv = 1;
                } else {
                    //否则将 timer_flag 置为 1，表示需要定时关闭请求
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        } else {
            //如果是其它值，则表示使用 proactor 模式，直接调用 request->process() 处理请求
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            request->process();
        }
    }
}

#endif
