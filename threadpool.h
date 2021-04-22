#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "http.h"
#include "sys/epoll.h"
#include "dirent.h"
#include "signal.h"

class ThreadPool
{
public:
    ThreadPool(int thread_num = 8, int max_request = 1000);
    ~ThreadPool();
    void add_task(int fd);
    void destroy_threadpool();//摧毁线程池
    void set_epfd(int epfd);
    int get_epfd();
    
public:
    pthread_t *m_threads;
    pthread_mutex_t m_tasks_lock;//线程池的锁
    pthread_cond_t m_empty_task;//任务队列为空的条件
    pthread_cond_t m_not_empty_task;//任务队列不为空的条件
private:
    static void *work(void *arg);
    void run();
    int beginnum = 1000;
    int max_job_num;//最大任务个数
    int job_num;//实际任务个数
    Http *m_tasks;//任务队列数组
    int job_push;
    int job_pop;
    int thr_num;
    int m_epfd;
    int shutdown;
};
#endif