/*************************************************************
*循环数组实现的阻塞队列，m_back = (m_back + 1) % m_max_size;
*线程安全，每个操作前都要先加互斥锁，操作完后，再解锁
**************************************************************/

#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "../lock/locker.h"

using namespace std;

//
template<class T>
class block_queue {
public:     //公有成员

    /**
     * @brief 构造函数
     * @param max_size
     */
    block_queue(int max_size = 1000) {
        if (max_size <= 0) {
            exit(-1);
        }

        m_max_size = max_size;
        m_array = new T[max_size];
        m_size = 0;
        m_front = -1;
        m_back = -1;
    }

    /**
     * @brief
     */
    void clear() {
        m_mutex.lock();
        m_size = 0;
        m_front = -1;
        m_back = -1;
        m_mutex.unlock();
    }

    /**
     * @brief 析构函数
     */
    ~block_queue() {
        m_mutex.lock();
        if (m_array != NULL)
            delete[] m_array;

        m_mutex.unlock();
    }

    /**
     * @brief 判断队列是否满了
     * @return
     */
    bool full() {
        m_mutex.lock();
        if (m_size >= m_max_size) {

            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    /**
     * @brief 判断队列是否为空
     * @return
     */
    bool empty() {
        m_mutex.lock();
        if (0 == m_size) {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    /**
     * @brief 返回队首元素
     * @param value
     * @return
     */
    bool front(T &value) {
        m_mutex.lock();
        if (0 == m_size) {
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_front];
        m_mutex.unlock();
        return true;
    }

    /**
     * @brief 返回队尾元素
     * @param value
     * @return
     */
    bool back(T &value) {
        m_mutex.lock();
        if (0 == m_size) {
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_back];
        m_mutex.unlock();
        return true;
    }

    /**
     * @brief
     * @return
     */
    int size() {
        int tmp = 0;

        m_mutex.lock();
        tmp = m_size;

        m_mutex.unlock();
        return tmp;
    }

    /**
     * @brief
     * @return
     */
    int max_size() {
        int tmp = 0;

        m_mutex.lock();
        tmp = m_max_size;

        m_mutex.unlock();
        return tmp;
    }

    /**
     * @brief 往队列添加元素，需要将所有使用队列的线程先唤醒
     * 当有元素push进队列,相当于生产者生产了一个元素
     * 若当前没有线程等待条件变量,则唤醒无意义
     * @param item
     * @return
     */
    bool push(const T &item) {

        m_mutex.lock();
        if (m_size >= m_max_size) {

            m_cond.broadcast();
            m_mutex.unlock();
            return false;
        }

        m_back = (m_back + 1) % m_max_size;
        m_array[m_back] = item;

        m_size++;

        m_cond.broadcast();
        m_mutex.unlock();
        return true;
    }

    /**
     * @brief pop时,如果当前队列没有元素,将会等待条件变量
     * @param item
     * @return
     */
    bool pop(T &item) {

        m_mutex.lock();
        while (m_size <= 0) {

            if (!m_cond.wait(m_mutex.get())) {
                m_mutex.unlock();
                return false;
            }
        }

        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }

    /**
     * @brief 增加了超时处理
     * @param item
     * @param ms_timeout
     * @return
     */
    bool pop(T &item, int ms_timeout) {
        struct timespec t = {0, 0};
        struct timeval now = {0, 0};
        gettimeofday(&now, NULL);
        m_mutex.lock();
        if (m_size <= 0) {
            t.tv_sec = now.tv_sec + ms_timeout / 1000;
            t.tv_nsec = (ms_timeout % 1000) * 1000;
            if (!m_cond.timewait(m_mutex.get(), t)) {
                m_mutex.unlock();
                return false;
            }
        }

        if (m_size <= 0) {
            m_mutex.unlock();
            return false;
        }

        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }

private:    //私有成员
    locker m_mutex;
    cond m_cond;

    T *m_array;
    int m_size;
    int m_max_size;
    int m_front;
    int m_back;
};

#endif
