#include "threadpool.h"
#include <iostream>
//char pwd_path[256]="";

ThreadPool::ThreadPool(int thread_num, int max_tasknum)
{
    
    printf("begin call %s-----\n",__FUNCTION__);
    thr_num=thread_num;
    max_job_num=max_tasknum;
    shutdown = 0;//是否摧毁线程池，1代表摧毁
    job_push = 0;//任务队列添加的位置
    job_pop = 0;//任务队列出队的位置
    job_num = 0;//初始化的任务个数为0
    m_tasks = new Http[max_job_num];

    //初始化锁和条件变量
    if(pthread_mutex_init(&m_tasks_lock,NULL)!=0)
        throw std::exception();
    if(pthread_cond_init(&m_empty_task,NULL)!=0)
        throw std::exception();
    if(pthread_cond_init(&m_not_empty_task,NULL)!=0)
        throw std::exception();

    int i = 0;
    m_threads = new pthread_t[thr_num];//申请n个线程id的空间
	
    for(i = 0;i < thr_num;++i)
	{
        if (pthread_create(m_threads + i, NULL, work, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

ThreadPool::~ThreadPool(){}

void *ThreadPool::work(void *arg)
{
    printf("begin call %s-----\n",__FUNCTION__);
    ThreadPool *pool = (ThreadPool *)arg;
    pool->run();
    return pool;
}

void ThreadPool::set_epfd(int epfd)
{
    m_epfd=epfd;
}
int ThreadPool::get_epfd()
{
    return m_epfd;
}
void ThreadPool::run()
{
    printf("begin call %s-----\n",__FUNCTION__);
    int task_pos = 0;//任务位置

    while(1)
    {
        Http *task=nullptr;
        //获取任务，先要尝试加锁
        pthread_mutex_lock(&m_tasks_lock);
        while(job_num<=0&&shutdown!=0)
        {
            //
            pthread_cond_wait(&m_not_empty_task,&m_tasks_lock);
        }
        if(job_num)
        {
            //有任务需要处理
            task_pos = (job_pop++)%max_job_num;
            //printf("task out %d...tasknum===%d tid=%lu\n",taskpos,thrPool->tasks[taskpos].tasknum,pthread_self());
			//为什么要拷贝？避免任务被修改，生产者会添加任务
            task = m_tasks+task_pos;
            job_num--;
            //task = &thrPool->tasks[taskpos];
            pthread_cond_signal(&m_empty_task);//通知生产者
        }
        if(shutdown)
		{
            //代表要摧毁线程池，此时线程退出即可
            //pthread_detach(pthread_self());//临死前分家
            pthread_mutex_unlock(&m_tasks_lock);
			pthread_exit(NULL);
        }
        pthread_mutex_unlock(&m_tasks_lock);
        if(task==nullptr)
            continue;
        else
        {
            printf("001\n");
            task->process();//执行回调函数
            printf("002\n");
        }
    }
}

void ThreadPool::destroy_threadpool()
{
    shutdown = 1;//开始自爆
    pthread_cond_broadcast(&m_not_empty_task);//诱杀 

    int i = 0;
    for(i = 0; i < thr_num ; i++)
	{
        pthread_join(m_threads[i],NULL);
    }

    pthread_cond_destroy(&m_not_empty_task);
    pthread_cond_destroy(&m_empty_task);
    pthread_mutex_destroy(&m_tasks_lock);

    delete[] m_tasks;
    delete[] m_threads;
}

//添加任务到线程池
void ThreadPool::add_task(int fd)
{
    printf("begin call %s-----\n",__FUNCTION__);
    pthread_mutex_lock(&m_tasks_lock);

	//实际任务总数大于最大任务个数则阻塞等待(等待任务被处理)
    while(max_job_num <= job_num)
	{
        pthread_cond_wait(&m_empty_task,&m_tasks_lock);
    }

    int task_pos = (job_push++)%max_job_num;
    printf("add task %d  tasknum===%d\n",task_pos,beginnum);
    m_tasks[task_pos].tasknum=beginnum++;
    printf("asdf\n");
    m_tasks[task_pos].set_cfd(fd);
    m_tasks[task_pos].set_epfd(get_epfd());
    // (m_tasks+task_pos)->set_event(evs);
    job_num++;

    pthread_mutex_unlock(&m_tasks_lock);

    pthread_cond_signal(&m_not_empty_task);//通知包身工
    // printf("end call %s-----\n",__FUNCTION__);
}