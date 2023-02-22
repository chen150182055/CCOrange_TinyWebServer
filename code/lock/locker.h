#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

//sem类
class sem {
public:     //公有成员
    /**
     * 构造函数
     */
    sem() {
        if (sem_init(&m_sem, 0, 0) != 0) {
            throw std::exception();
        }
    }

    /**
     * 构造函数
     * @param num
     */
    sem(int num) {
        if (sem_init(&m_sem, 0, num) != 0) {
            throw std::exception();
        }
    }

    /**
     * 析构函数
     */
    ~sem() {
        sem_destroy(&m_sem);
    }

    /**
     *
     * @return
     */
    bool wait() {
        return sem_wait(&m_sem) == 0;
    }

    /**
     *
     * @return
     */
    bool post() {
        return sem_post(&m_sem) == 0;
    }

private:    //私有成员
    sem_t m_sem;
};

//locker类
class locker {
public:     //公有成员

    /**
     * 构造函数
     */
    locker() {
        if (pthread_mutex_init(&m_mutex, NULL) != 0) {
            throw std::exception();
        }
    }

    /**
     * @brief 析构函数
     */
    ~locker() {
        pthread_mutex_destroy(&m_mutex);
    }

    /**
     * @brief
     * @return
     */
    bool lock() {
        return pthread_mutex_lock(&m_mutex) == 0;
    }

    /**
     *
     * @return
     */
    bool unlock() {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    /**
     *
     * @return
     */
    pthread_mutex_t *get() {
        return &m_mutex;
    }

private:    //私有成员
    pthread_mutex_t m_mutex;
};

//cond类
class cond {
public:     //公有成员

    /**
     * 构造函数
     */
    cond() {
        if (pthread_cond_init(&m_cond, NULL) != 0) {
            //pthread_mutex_destroy(&m_mutex);
            throw std::exception();
        }
    }

    /**
     * 析构函数
     */
    ~cond() {
        pthread_cond_destroy(&m_cond);
    }

    /**
     * @brief
     * @param m_mutex
     * @return
     */
    bool wait(pthread_mutex_t *m_mutex) {
        int ret = 0;
        //pthread_mutex_lock(&m_mutex);
        ret = pthread_cond_wait(&m_cond, m_mutex);
        //pthread_mutex_unlock(&m_mutex);
        return ret == 0;
    }

    /**
     * @brief
     * @param m_mutex
     * @param t
     * @return
     */
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t) {
        int ret = 0;
        //pthread_mutex_lock(&m_mutex);
        ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
        //pthread_mutex_unlock(&m_mutex);
        return ret == 0;
    }

    /**
     * @brief
     * @return
     */
    bool signal() {
        return pthread_cond_signal(&m_cond) == 0;
    }

    /**
     * @brief
     * @return
     */
    bool broadcast() {
        return pthread_cond_broadcast(&m_cond) == 0;
    }

private:    //私有成员
    //static pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
};

#endif
