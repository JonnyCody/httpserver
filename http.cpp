#include "http.h"

// Http::Http(int epfd,struct epoll_event *ev):m_epfd(epfd),m_ev(ev){}

void Http::send_header(int code,char *info,char *filetype,int length)
{	//发送状态行
	char buf[WRITE_BUFFER_SIZE]="";
	int len =0;
	len = sprintf(buf,"HTTP/1.1 %d %s\r\n",code,info);
	send(m_cfd,buf,len,0);
	//发送消息头
	len = sprintf(buf,"Content-Type:%s\r\n",filetype);
	send(m_cfd,buf,len,0);
	if(length > 0)
	{
		//发送消息头
		len = sprintf(buf,"Content-Length:%d\r\n",length);
		send(m_cfd,buf,len,0);

	}
	//空行
	send(m_cfd,"\r\n",2,0);
}

void Http::set_epfd(int epfd)
{
	m_epfd=epfd;
}

void Http::set_cfd(int cfd)
{
	m_cfd=cfd;
}

void Http::send_file(char *path,int flag)
{
	int fd = open(path,O_RDONLY);
	if(fd <0)
	{
		perror("");
		return ;
	}
	char buf[WRITE_BUFFER_SIZE]="";
	int len =0;
	while( 1 )
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
			n = send(m_cfd,buf,len,0);
			printf("len=%d\n", n);
		}
	}
	close(fd);
	//关闭cfd,下树
	if(flag==1)
	{
		close(m_cfd);
		epoll_ctl(m_epfd,EPOLL_CTL_DEL,m_cfd,0);
	}
}

void Http::process()
{
	// read_back();
	read_client_request();
}

void Http::reset_oneshot()
{
	struct epoll_event event;
	event.data.fd=m_cfd;
	event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
	epoll_ctl(m_epfd,EPOLL_CTL_MOD,m_cfd,&event);
}

void Http::read_back()
{
    printf("003\n");
    char buf[READ_BUFFER_SIZE];
	memset(buf, '\0',READ_BUFFER_SIZE);
	int n=0;
	while(1)
	{
		int ret=recv(m_cfd,buf,1024-1,0);
		n+=ret;
		if(ret==0)
		{
			close(m_cfd);
			printf("foreiner closed the connection\n");
			break;
		}
		else if(ret < 0)
		{
			if(errno == EAGAIN)
			{
				reset_oneshot();
				printf("read later\n");
				break;
			}
		}
		else
		{
			printf("%s\n",buf);
			write(m_cfd ,buf,n);
		}
	}
 	printf("004\n");
}

void Http::read_client_request()
{
    // printf("%s\n",getenv("PWD"));
	//读取请求(先读取一行,在把其他行读取,扔掉)
	printf("%s\n",getenv("PWD"));
	memset(m_read_buf,'\0',READ_BUFFER_SIZE);
	while(1)
	{
		int ret=recv(m_cfd,m_read_buf,READ_BUFFER_SIZE-1,0);
		if(ret==0)
		{
			close(m_cfd);
			printf("foreiner closed the connection\n");
			break;
		}
		else if(ret < 0)
		{
			if(errno == EAGAIN)
			{
				reset_oneshot();
				printf("read later\n");
				break;
			}
		}
		else
		{
			printf("%s\n",m_read_buf);
			char method[FILENAME_LEN];
			char content[FILENAME_LEN];
			char protocol[FILENAME_LEN];
			sscanf(m_read_buf,"%[^ ] %[^ ] %[^ \r\n]",method,content,protocol);
			printf("[%s]  [%s]  [%s]\n",method,content,protocol );
			//判断是否为get请求  get   GET
			if( strcasecmp(method,"get") == 0)
			{
				//[GET]  [/%E8%8B%A6%E7%93%9C.txt]  [HTTP/1.1]
				char* strfile=content+1;
				strdecode(strfile,strfile);
				//GET / HTTP/1.1\R\N
				//如果没有请求文件,默认请求当前目录
				if( *strfile== 0)
					strfile= "./";
				//判断请求的文件在不在
				struct stat s;
				if(stat(strfile,&s)< 0)//文件不存在
				{
					printf("file not fount\n");
					//先发送 报头(状态行  消息头  空行)
					send_header( 404,"NOT FOUND",get_mime_type("*.html"),0);
					//发送文件 error.html
					send_file("error.html",1);
				}
				else
				{
					//请求的是一个普通的文件
					if(S_ISREG(s.st_mode))
					{
						printf("file\n");
						//先发送 报头(状态行  消息头  空行)
						send_header(200,"OK",get_mime_type(strfile),s.st_size);
						//发送文件
						send_file(strfile,1);
					}
					else if(S_ISDIR(s.st_mode))//请求的是一个目录
					{
						printf("dir\n");
						//发送一个列表  网页
						send_header(200,"OK",get_mime_type("*.html"),0);
						//发送header.html
						send_file("dir_header.html",0);
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
							send(m_cfd,buf,len ,0);
							free(mylist[i]);
						}
						free(mylist);
						send_file("dir_tail.html",1);
					}
				}
			}
		}
	}
}