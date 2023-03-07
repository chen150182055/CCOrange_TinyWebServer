#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"

using namespace std;

//Log类,实现日志记录功能
class Log {
public:     //公有成员

    /**
     * @brief 返回唯一的Log实例，使用单例模式
     * @return
     */
    static Log *get_instance() {
        static Log instance;
        return &instance;
    }

    /**
     * @brief 静态函数，周期性地将日志队列中的消息写入日志文件中
     * @param args
     * @return
     */
    static void *flush_log_thread(void *args) {
        //async_write_log()函数是用于异步写入日志的函数，它会将日志消息添加到队列中，并将队列中的日志消息逐一写入日志文件中
        Log::get_instance()->async_write_log();
        //样，当多个线程同时写入日志时，它们不会阻塞彼此，而是将日志消息添加到队列中等待被写入文件
    }

    bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000,int max_queue_size = 0);

    void write_log(int level, const char *format, ...);

    void flush(void);

private:
    Log();

    virtual ~Log();

    /**
     * @brief 步写日志，通过阻塞队列实现
     * @return
     */
    void *async_write_log() {
        string single_log;
        //从阻塞队列中取出一个日志string，写入文件
        while (m_log_queue->pop(single_log)) {
            m_mutex.lock();
            fputs(single_log.c_str(), m_fp);
            m_mutex.unlock();
        }
    }

private:
    char dir_name[128]; //路径名
    char log_name[128]; //log文件名
    int m_split_lines;  //日志最大行数
    int m_log_buf_size; //日志缓冲区大小
    long long m_count;  //日志行数记录
    int m_today;        //因为按天分类,记录当前时间是那一天
    FILE *m_fp;         //打开log的文件指针
    char *m_buf;
    block_queue<string> *m_log_queue; //阻塞队列
    bool m_is_async;                  //是否同步标志位
    locker m_mutex;                   //互斥锁
    int m_close_log;                  //关闭日志
};

#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}

#endif
