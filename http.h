#ifndef HTTP_H
#define HTTP_H
#include "stdio.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <dirent.h>
#include <string>
#include "pub.h"

class Http
{
public:
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    static const int FILENAME_LEN = 200;
    int tasknum;

public:
    // Http(int epfd,struct epoll_event *ev){}
    Http(){}
    ~Http(){}
public:
    void init(int sockfd,const sockaddr_in &addr);
    void process();
    void set_epfd(int epfd);
    void set_cfd(int cfd);

public:
    void read_back();
    void read_client_request();
    void send_header(int code,char *info,char *filetype,int length);
    void send_file(char *path,int epfd);
    void reset_oneshot();

private:
    struct sockaddr_in m_address;
    char m_read_buf[READ_BUFFER_SIZE];
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_epfd;
    int m_cfd;
};

#endif