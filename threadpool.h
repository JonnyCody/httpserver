#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include "sys/epoll.h"
#include "wrap.h"

typedef struct _PoolTask
{
    int tasknum;//模拟任务编号
    void *arg;
    void (*task_func)(void *arg);//任务的回调函数
    int fd;
    int epfd;
    struct epoll_event *evs;
}PoolTask;

typedef struct _ThreadPool
{
    int max_job_num;//最大任务个数
    int job_num;//实际任务个数
    PoolTask *tasks;//任务队列数组
    int job_push;
    int job_pop;
    int thr_num;
    pthread_t *threads;
    int shutdown;
    pthread_mutex_t pool_lock;//线程池的锁
    pthread_cond_t empty_task;//任务队列为空的条件
    pthread_cond_t not_empty_task;//任务队列不为空的条件
}ThreadPool;

void create_threadpool(int thr_num,int max_tasknum);//创建线程池--thrnum  代表线程个数，maxtasknum 最大任务个数
void destroy_threadpool(ThreadPool *pool);//摧毁线程池
void add_task(ThreadPool *pool,int fd,struct epoll_event *evs);
void task_run(void *arg);//任务回调函数

#endif