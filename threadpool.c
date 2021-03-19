#include "threadpool.h"
#include "stdio.h"
#include "wrap.h"
#include "sys/epoll.h"
#include <fcntl.h>
#include <sys/stat.h>
#include "pub.h"
#include "dirent.h"
#include "signal.h"
#define PORT 8889
#define READ_BUFFER_SIZE  2048
#define WRITE_BUFFER_SIZE  1024
#define FILENAME_LEN  256

//char pwd_path[256]="";

ThreadPool *thr_pool=NULL;

int beginnum=1000;

void send_header(int cfd, int code,char *info,char *filetype,int length)
{	//发送状态行
	char buf[WRITE_BUFFER_SIZE]="";
	int len =0;
	len = sprintf(buf,"HTTP/1.1 %d %s\r\n",code,info);
	send(cfd,buf,len,0);
	//发送消息头
	len = sprintf(buf,"Content-Type:%s\r\n",filetype);
	send(cfd,buf,len,0);
	if(length > 0)
	{
		//发送消息头
		len = sprintf(buf,"Content-Length:%d\r\n",length);
		send(cfd,buf,len,0);

	}
	//空行
	send(cfd,"\r\n",2,0);
}

void send_file(int cfd,char *path,struct epoll_event *ev,int epfd,int flag)
{
	int fd = open(path,O_RDONLY);
	if(fd <0)
	{
		perror("");
		return ;
	}
	char buf[WRITE_BUFFER_SIZE]="";
	int len =0;
	while( 1)
	{
		len = read(fd,buf,sizeof(buf));
		if(len < 0)
		{
			perror("");
			break;

		}
		else if(len == 0)
		{
			break;
		}
		else
		{
			int n=0;
			n =  send(cfd,buf,len,0);
			printf("len=%d\n", n);
		}
	}
	close(fd);
	//关闭cfd,下树
	if(flag==1)
	{
		close(cfd);
		epoll_ctl(epfd,EPOLL_CTL_DEL,cfd,ev);
	}
}

void *thr_run(void *arg)
{
    ThreadPool *pool = (ThreadPool*)arg;
    int task_pos = 0;//任务位置
    PoolTask *task = (PoolTask *)malloc(sizeof(PoolTask));
    while(1)
    {
        //获取任务，先要尝试加锁
        pthread_mutex_lock(&thr_pool->pool_lock);
        while(thr_pool->job_num<=0&&!thr_pool->shutdown)
        {
            //
            pthread_cond_wait(&thr_pool->not_empty_task,&thr_pool->pool_lock);
        }
        if(thr_pool->job_num)
        {
            //有任务需要处理
            task_pos = (thr_pool->job_pop++)%thr_pool->max_job_num;
            //printf("task out %d...tasknum===%d tid=%lu\n",taskpos,thrPool->tasks[taskpos].tasknum,pthread_self());
			//为什么要拷贝？避免任务被修改，生产者会添加任务
            memcpy(task,&thr_pool->tasks[task_pos],sizeof(PoolTask));
            task->arg = task;
            thr_pool->job_num--;
            //task = &thrPool->tasks[taskpos];
            pthread_cond_signal(&thr_pool->empty_task);//通知生产者
        }

        if(thr_pool->shutdown)
		{
            //代表要摧毁线程池，此时线程退出即可
            //pthread_detach(pthread_self());//临死前分家
            pthread_mutex_unlock(&thr_pool->pool_lock);
            free(task);
			pthread_exit(NULL);
        }

        pthread_mutex_unlock(&thr_pool->pool_lock);
        printf("001\n");
        task->task_func(task->arg);//执行回调函数
        printf("002\n");
    }
}

void create_threadpool(int thrnum,int maxtasknum)
{
    printf("begin call %s-----\n",__FUNCTION__);
    thr_pool = (ThreadPool*)malloc(sizeof(ThreadPool));

    thr_pool->thr_num = thrnum;
    thr_pool->max_job_num = maxtasknum;
    thr_pool->shutdown = 0;//是否摧毁线程池，1代表摧毁
    thr_pool->job_push = 0;//任务队列添加的位置
    thr_pool->job_pop = 0;//任务队列出队的位置
    thr_pool->job_num = 0;//初始化的任务个数为0

    thr_pool->tasks = (PoolTask*)malloc((sizeof(PoolTask)*maxtasknum));//申请最大的任务队列

    //初始化锁和条件变量
    pthread_mutex_init(&thr_pool->pool_lock,NULL);
    pthread_cond_init(&thr_pool->empty_task,NULL);
    pthread_cond_init(&thr_pool->not_empty_task,NULL);

    int i = 0;
    thr_pool->threads = (pthread_t *)malloc(sizeof(pthread_t)*thrnum);//申请n个线程id的空间
	
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    for(i = 0;i < thrnum;++i)
	{
        pthread_create(&thr_pool->threads[i],&attr,thr_run,(void*)thr_pool);//创建多个线程
    }
    //printf("end call %s-----\n",__FUNCTION__);
}

void destroy_threadpool(ThreadPool *pool)
{
    pool->shutdown = 1;//开始自爆
    pthread_cond_broadcast(&pool->not_empty_task);//诱杀 

    int i = 0;
    for(i = 0; i < pool->thr_num ; i++)
	{
        pthread_join(pool->threads[i],NULL);
    }

    pthread_cond_destroy(&pool->not_empty_task);
    pthread_cond_destroy(&pool->empty_task);
    pthread_mutex_destroy(&pool->pool_lock);

    free(pool->tasks);
    free(pool->threads);
    free(pool);
}
//添加任务到线程池
void addtask(ThreadPool *pool,int fd,struct epoll_event *evs)
{
    //printf("begin call %s-----\n",__FUNCTION__);
    pthread_mutex_lock(&pool->pool_lock);

	//实际任务总数大于最大任务个数则阻塞等待(等待任务被处理)
    while(pool->max_job_num <= pool->job_num)
	{
        pthread_cond_wait(&pool->empty_task,&pool->pool_lock);
    }

    int task_pos = (pool->job_push++)%pool->max_job_num;
    //printf("add task %d  tasknum===%d\n",taskpos,beginnum);
    pool->tasks[task_pos].tasknum = beginnum++;
    pool->tasks[task_pos].arg = (void*)&pool->tasks[task_pos];
    pool->tasks[task_pos].task_func = task_run;
    pool->tasks[task_pos].fd = fd;
    pool->tasks[task_pos].evs = evs;
    pool->job_num++;

    pthread_mutex_unlock(&pool->pool_lock);

    pthread_cond_signal(&pool->not_empty_task);//通知包身工
    //printf("end call %s-----\n",__FUNCTION__);
}

void reset_oneshot(int epollfd, int fd)
{
	struct epoll_event event;
	event.data.fd=fd;
	event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
	epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}

void read_back(int epfd,struct epoll_event *ev)
{
    printf("003\n");
    char buf[READ_BUFFER_SIZE];
	memset(buf, '\0',READ_BUFFER_SIZE);
	int n=0;
	while(1)
	{
		int ret=recv(ev->data.fd,buf,1024-1,0);
		n+=ret;
		if(ret==0)
		{
			close(ev->data.fd);
			printf("foreiner closed the connection\n");
			break;
		}
		else if(ret < 0)
		{
			if(errno == EAGAIN)
			{
				reset_oneshot(epfd,ev->data.fd);
				printf("read later\n");
				break;
			}
		}
		else
		{	
			printf("%s\n",buf);
			Write(ev->data.fd ,buf,n);
		}
	}
 	printf("004\n");
}

void read_client_request(int epfd ,struct epoll_event *ev)
{
    printf("%s\n",getenv("PWD"));
	//读取请求(先读取一行,在把其他行读取,扔掉)
	char buf[READ_BUFFER_SIZE];
	memset(buf,'\0',READ_BUFFER_SIZE);
	int len=0;
	while(1)
	{
		int ret=recv(ev->data.fd,buf,1024-1,0);
		len+=ret;
		if(ret==0)
		{
			close(ev->data.fd);
			printf("foreiner closed the connection\n");
			break;
		}
		else if(ret < 0)
		{
			if(errno == EAGAIN)
			{
				reset_oneshot(epfd,ev->data.fd);
				printf("read later\n");
				break;
			}
		}
		else
		{
			printf("%s\n",buf);
			char method[FILENAME_LEN];
			char content[FILENAME_LEN];
			char protocol[FILENAME_LEN];
			sscanf(buf,"%[^ ] %[^ ] %[^ \r\n]",method,content,protocol);
			printf("[%s]  [%s]  [%s]\n",method,content,protocol );
			printf("[%s]  [%s]  [%s]\n",method,content,protocol );

			if( strcasecmp(method,"get") == 0)
			{
				//[GET]  [/%E8%8B%A6%E7%93%9C.txt]  [HTTP/1.1]
				char *strfile = content+1;
				strdecode(strfile,strfile);
				//GET / HTTP/1.1\R\N
				//如果没有请求文件,默认请求当前目录
				if(*strfile == 0)
					strfile= "./";
				//判断请求的文件在不在
				struct stat s;
				if(stat(strfile,&s)< 0)//文件不存在
				{
					printf("file not fount\n");
					//先发送 报头(状态行  消息头  空行)
					send_header(ev->data.fd, 404,"NOT FOUND",get_mime_type("*.html"),0);
					//发送文件 error.html
					send_file(ev->data.fd,"error.html",ev,epfd,1);
				}
				else
				{
					//请求的是一个普通的文件
					if(S_ISREG(s.st_mode))
					{
						printf("file\n");
						//先发送 报头(状态行  消息头  空行)
						send_header(ev->data.fd, 200,"OK",get_mime_type(strfile),s.st_size);
						//发送文件
						send_file(ev->data.fd,strfile,ev,epfd,1);
					}
					else if(S_ISDIR(s.st_mode))//请求的是一个目录
					{
						printf("dir\n");
						//发送一个列表  网页
						send_header(ev->data.fd, 200,"OK",get_mime_type("*.html"),0);
						//发送header.html
						send_file(ev->data.fd,"dir_header.html",ev,epfd,0);
						struct dirent **mylist=NULL;
						char buf[1024]="";
						int len =0;
						int n = scandir(strfile,&mylist,NULL,alphasort);
						for(int i=0;i<n;i++)
						{
							//printf("%s\n", mylist[i]->d_name);
							if(mylist[i]->d_type == DT_DIR)//如果是目录
							{
								len = sprintf(buf,"<li><a href=%s/ >%s</a></li>",mylist[i]->d_name,mylist[i]->d_name);
							}
							else
							{
								len = sprintf(buf,"<li><a href=%s >%s</a></li>",mylist[i]->d_name,mylist[i]->d_name);
							}
							send(ev->data.fd,buf,len ,0);
							free(mylist[i]);
						}
						free(mylist);
						send_file(ev->data.fd,"dir_tail.html",ev,epfd,1);
					}
				}
			}
		}
	}
}


//任务回调函数
void task_run(void *arg)
{
    printf("003\n");
    
    PoolTask *task = (PoolTask*)arg;
	// read_back(task->epfd ,task->evs);
    read_client_request(task->epfd ,task->evs);

	// printf("003\n");
	// PoolTask *task = (PoolTask*)arg;
	// char buf[1024]="";
	// int n = Read(task->fd , buf,sizeof(buf));
	// if(n == 0 )
	// {
	// 	close(task->fd);//关闭cfd
	// 	epoll_ctl(task->epfd,EPOLL_CTL_DEL,task->fd,task->evs);//将cfd上树
	// 	printf("client close\n");
	// }
	// else if(n> 0)
	// {
	// 	printf("%s\n",buf );
	// 	Write(task->fd ,buf,n);
	// }
	// printf("004\n");
}



int main()
{
    signal(SIGPIPE,SIG_IGN);
	//切换工作目录
	//获取当前目录的工作路径
	char pwd_path[256]="";
	char * path = getenv("PWD");
	///home/itheima/share/bjc++34/07day/web-http
	strcpy(pwd_path,path);
	strcat(pwd_path,"/web-http");
	chdir(pwd_path);
	create_threadpool(4,20);
	
    int i = 0;
    int lfd=tcp4bind(PORT,NULL);
    //监听
    listen(lfd,128);
    //创建树
    int epfd = epoll_create(1);
    struct epoll_event ev,evs[1024];
    ev.data.fd = lfd;
    ev.events = EPOLLIN;//监听读事件
    //将ev上树
    epoll_ctl(epfd,EPOLL_CTL_ADD,lfd,&ev);
    while(1)
    {
        int nready = epoll_wait(epfd,evs,1024,-1);
        if(nready < 0)
            perr_exit("err");
        else if(nready == 0)
            continue;
        else if(nready>0)
        {
            for(int i=0;i<nready;++i)
            {
                if(evs[i].data.fd == lfd && evs[i].events & EPOLLIN)
                {
                    struct sockaddr_in cliaddr;
                    char buf_ip[16]="";
                    socklen_t len  = sizeof(cliaddr);
                    int cfd = Accept(lfd,(struct sockaddr *)&cliaddr,&len);
                    printf("client ip=%s port=%d\n",inet_ntop(AF_INET,
                    &cliaddr.sin_addr.s_addr,buf_ip,sizeof(buf_ip)),
                    ntohs(cliaddr.sin_port));
                    ev.data.fd = cfd;//cfd上树
                    ev.events = EPOLLIN | EPOLLONESHOT | EPOLLRDHUP;//监听读事件
					// ev.events = EPOLLIN;
                    epoll_ctl(epfd,EPOLL_CTL_ADD,cfd,&ev);//将cfd上树
                }
                else if(evs[i].events & EPOLLIN)//普通读事件
                {
                    addtask(thr_pool,evs[i].data.fd,&evs[i]);
                }
            }
            
        }
    }
}