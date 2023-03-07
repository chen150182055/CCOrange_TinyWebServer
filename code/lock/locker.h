#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

//sem类
class sem {
public:
    /**
     * @brief 构造函数
     */
    sem() {
        if (sem_init(&m_sem, 0, 0) != 0) {
            throw std::exception();
        }
    }

    /**
     * @brief 构造函数
     * @param num
     */
    sem(int num) {
        if (sem_init(&m_sem, 0, num) != 0) {
            throw std::exception();
        }
    }

    /**
     * @brief 析构函数
     */
    ~sem() {
        sem_destroy(&m_sem);
    }

    /**
     * @brief 等待信号量的值变为非零，然后将信号量值减1
     * @return
     */
    bool wait() {
        //调用C标准库中的sem_wait函数，该函数会将信号量的值减1，如果信号量的值为0，则会阻塞等待
        return sem_wait(&m_sem) == 0;
    }

    /**
     * @brief 将一个指定的信号量的值加 1
     * @return
     */
    bool post() {
        //将一个指定的信号量的值加 1
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem;
};

//locker类
class locker {
public:

    /**
     * @brief 构造函数
     */
    locker() {
        //它的第一个参数是一个指向互斥锁的指针，第二个参数是一个指向互斥锁属性的指针，如果为 NULL，则使用默认属性
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
     * @brief 是否成功地对互斥锁进行了加锁操作
     * @return
     */
    bool lock() {
        //它的参数是一个指向互斥锁的指针
        return pthread_mutex_lock(&m_mutex) == 0;
    }

    /**
     * @brief 是否成功地对互斥锁进行了解锁操作
     * @return
     */
    bool unlock() {
        //它的参数是一个指向互斥锁的指针
        // 函数会对指定的互斥锁进行解锁操作。如果解锁成功，则该函数返回 0
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    /**
     * @brief 获取互斥锁
     * @return
     */
    pthread_mutex_t *get() {
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;
};

//cond类
class cond {
public:

    /**
     * @brief 构造函数
     */
    cond() {
        if (pthread_cond_init(&m_cond, NULL) != 0) {
            //pthread_mutex_destroy(&m_mutex);
            throw std::exception();
        }
    }

    /**
     * @brief 析构函数
     */
    ~cond() {
        pthread_cond_destroy(&m_cond);
    }

    /**
     * @brief 等待条件变量，并在等待期间自动解锁互斥锁，以便其它线程可以获取该锁
     * @param m_mutex 指向互斥锁的指针
     * @return
     */
    bool wait(pthread_mutex_t *m_mutex) {
        int ret = 0;
        //pthread_mutex_lock(&m_mutex);
        //该函数会自动释放传入的互斥锁，然后将当前线程挂起直到条件变量被其它线程通知
        ret = pthread_cond_wait(&m_cond, m_mutex);
        //pthread_mutex_unlock(&m_mutex);
        return ret == 0;
    }

    /**
     * @brief 等待条件变量的函数，并且可以在一定时间内超时返回
     * 该函数与 wait() 函数的区别在于，它可以在一定时间内等待条件变量，并且不会一直阻塞线程
     * @param m_mutex 指向互斥锁的指针
     * @param t 表示等待超时的时间
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
     * @brief 通知等待在条件变量上的一个线程
     * @return
     */
    bool signal() {
        //函数会随机选择一个等待在条件变量上的线程进行通知
        return pthread_cond_signal(&m_cond) == 0;
    }

    /**
     * @brief 通知等待在条件变量上的所有线程
     * @return
     */
    bool broadcast() {
        //该函数会通知所有等待在条件变量上的线程
        return pthread_cond_broadcast(&m_cond) == 0;
    }

private:
    //static pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
};

#endif
