#include "threadpool.h"
#include "assert.h"
#define PORT 8889

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
    //启动8个线程，任务队列最多10
    ThreadPool tp(8,10);
	
    int i = 0;
    int lfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(lfd >= 0);

    //关闭连接
    struct linger tmp = {0, 1};
    setsockopt(lfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    int ret=0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);

    int flag = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(lfd, (struct sockaddr *)&address, sizeof(address));
    assert(lfd >= 0);
    ret = listen(lfd, 5);
    assert(lfd >= 0);
    //创建树
    int epfd = epoll_create(5);
    printf("epoll create\n");
    epoll_event ev,evs[1024];
    ev.data.fd = lfd;
    ev.events = EPOLLIN;//监听读事件
    //将ev上树
    epoll_ctl(epfd,EPOLL_CTL_ADD,lfd,&ev);
    tp.set_epfd(epfd);
    while(1)
    {
        int nready = epoll_wait(epfd,evs,1024,-1);
        if(nready < 0)
            printf("epoll failure");
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
                    int cfd = accept(lfd,(struct sockaddr *)&cliaddr,&len);
                    if(cfd<0)
                        printf("%s:errno is:%d", "accept error", errno);
                    else
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
                    // printf("add new task\n");
                    tp.add_task(evs[i].data.fd);
                }
            }
        }
    }
}