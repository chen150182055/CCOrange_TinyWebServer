#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "log.h"
#include <pthread.h>

using namespace std;

/**
 * @brief 构造函数
 */
Log::Log() {
    m_count = 0;
    m_is_async = false;
}

/**
 * @brief 析构函数
 */
Log::~Log() {
    if (m_fp != NULL) {
        fclose(m_fp);
    }
}

//异步需要设置阻塞队列的长度，同步不需要设置
/**
 * @brief 初始化
 * @param file_name
 * @param close_log
 * @param log_buf_size
 * @param split_lines
 * @param max_queue_size
 * @return
 */
bool Log::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size) {
    //如果设置了max_queue_size,则设置为异步
    if (max_queue_size >= 1) {
        m_is_async = true;
        m_log_queue = new block_queue<string>(max_queue_size);
        pthread_t tid;
        //flush_log_thread为回调函数,这里表示创建线程异步写日志
        pthread_create(&tid, NULL, flush_log_thread, NULL);
    }

    m_close_log = close_log;
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size);
    m_split_lines = split_lines;

    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;


    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {0};

    if (p == NULL) {
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                 file_name);
    } else {
        strcpy(log_name, p + 1);
        strncpy(dir_name, file_name, p - file_name + 1);
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1,
                 my_tm.tm_mday, log_name);
    }

    m_today = my_tm.tm_mday;

    m_fp = fopen(log_full_name, "a");
    if (m_fp == NULL) {
        return false;
    }

    return true;
}

/**
 * @brief 写日志
 * @param level 整型的日志级别
 * @param format 写入的日志内容
 * @param ...
 */
void Log::write_log(int level, const char *format, ...) {
    struct timeval now = {0, 0};
    //通过gettimeofday()函数获取当前时间
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    //调用localtime()函数将秒数转换为当前时间的年、月、日、时、分和秒
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};
    //根据日志级别，选择对应的日志前缀字符串并保存在字符数组s中
    switch (level) {
        case 0:
            strcpy(s, "[debug]:");
            break;
        case 1:
            strcpy(s, "[info]:");
            break;
        case 2:
            strcpy(s, "[warn]:");
            break;
        case 3:
            strcpy(s, "[erro]:");
            break;
        default:
            strcpy(s, "[info]:");
            break;
    }
    //写入一个log，对m_count++, m_split_lines最大行数
    m_mutex.lock();
    m_count++;

    //如果当前日志行数达到了最大行数或者日期发生了变化，则重新打开一个新的日志文件
    if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0) //everyday log
    {

        char new_log[256] = {0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16] = {0};

        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

        if (m_today != my_tm.tm_mday) {
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        } else {
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
        }
        m_fp = fopen(new_log, "a");
    }

    m_mutex.unlock();

    va_list valst;
    va_start(valst, format);

    string log_str;
    m_mutex.lock();

    //写入的具体时间内容格式
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

    //通过vsnprintf()函数将日志内容格式化成字符串并保存在字符数组m_buf中
    int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, valst);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    log_str = m_buf;

    m_mutex.unlock();
    //如果m_is_async为真，并且m_log_queue队列没有满
    if (m_is_async && !m_log_queue->full()) {
        //将日志字符串压入阻塞队列m_log_queue中
        m_log_queue->push(log_str);
    } else {
        //直接写入到文件中
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }

    //使用va_end()函数释放可变参数列表
    va_end(valst);
}

/**
 * @brief 刷新缓冲区
 */
void Log::flush(void) {
    m_mutex.lock();
    //强制刷新写入流缓冲区
    fflush(m_fp);
    m_mutex.unlock();
}
