#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include "threadpool.h"
#include "pub.h"

const int N = 1024;
const int PORT = 10000;

void acceptConn(void* arg);
void communication(void* arg);

typedef struct sockInfo
{
    int epfd;
    int fd;
}sockInfo;

int main()
{
    signal(SIGPIPE, SIG_IGN);
    //切换工作目录
    char workdir[256] = {0};
    strcpy(workdir,getenv("PWD"));
    strcat(workdir,"/web-http");
    chdir(workdir);

    //创建套接字
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1)
    {
        perror("socket error");
        exit(1);
    }

    //绑定IP和端口
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    //server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    inet_pton(AF_INET, "192.168.10.129", &server_addr.sin_addr.s_addr);

    //设置端口复用
    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    //绑定
    int ret = bind(lfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (ret == -1)
    {
        perror("bind error");
        exit(1);
    }

    //监听
    ret = listen(lfd, 128);
    if (ret == -1)
    {
        perror("listen error");
        exit(1);
    }

    //建树
    int epfd = epoll_create(1);
    if (epfd == -1)
    {
        perror("epoll_create error");
        exit(1);
    }

    //让监听的文件描述符上树
    struct epoll_event ev;
    ev.events = EPOLLIN;     //检测lfd的读缓冲区
    ev.data.fd = lfd;
    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
    if (ret == -1)
    {
        perror("epoll_ctl error");
        exit(1);
    }

    //创建线程池
    ThreadPool* pool = new ThreadPool(3, 10);

    struct epoll_event evs[N];
    int size = sizeof(evs) / sizeof(struct epoll_event);
    //持续检测
    while (true)
    {
        int num = epoll_wait(epfd, evs, size, -1);
        printf("%d fd is change...\n", num);
        if (num == -1) 
        {
            perror("epool_wait");
            exit(1);
        }
        for (int i = 0; i < num; i ++)
        {
            int fd = evs[i].data.fd;
            sockInfo* info = new sockInfo;
            info->epfd = epfd;
            info->fd = fd;
            if (fd == lfd)
            {
                pool->addTask(Task(acceptConn, info));
            }
            else
            {
                pool->addTask(Task(communication, info));
            }
        }
    }

    close(lfd);
    close(epfd);
    return 0;
}

void acceptConn(void* arg)  
{
    //epoll树会自动检测文件描述符的变化，不需要再while(true)
    sockInfo* info = (sockInfo*)arg;
    struct sockaddr_in cliaddr;
    char ip[16] = {0};
    socklen_t len = sizeof(cliaddr);
    int cfd = accept(info->fd ,(struct sockaddr*)&cliaddr ,&len);
    printf("new client ip=%s port=%d\n",
            inet_ntop(AF_INET, &cliaddr.sin_addr.s_addr, ip, 16),
            ntohs(cliaddr.sin_port));
    //设置非堵塞属性
    int flag = fcntl(cfd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);
    //设置监视读缓冲区边沿触发
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = cfd;
    //将事件上树
    epoll_ctl(info->epfd, EPOLL_CTL_ADD, cfd, &ev);
}


void communication(void* arg)
{
    sockInfo* info = (sockInfo*)arg;

    //读取请求(先读取一行,在把其他行读取,扔掉)
    char buf[N] = {0};
    char tmp[N] = {0};
    int n = Readline(info->fd, buf, sizeof(buf));
    if (n <= 0)
    {
        printf("close or err\n");
        epoll_ctl(info->epfd, EPOLL_CTL_DEL, info->fd, NULL);
        close(info->fd);
        return;
    }
    printf("[%s]\n", buf);
    int ret = 0;
    while ((ret = Readline(info->fd, tmp, sizeof(tmp))) > 0);

    //解析请求行  GET /a.txt  HTTP/1.1\r\n
    char method[256] = {0};
    char content[256] = {0};
    char protocol[256] = {0};
    sscanf(buf, "%[^ ] %[^ ] %[^ \r\n]", method, content, protocol);
    printf("[%s]  [%s]  [%s]\n", method, content, protocol);

    if (strcmp(method, "get") == 0 || strcmp(method, "GET") == 0)
    {
        char* strfile = content + 1;
        strdecode(strfile, strfile);  //中文转换
        if (*strfile == 0)//如果没有请求文件,默认请求当前目录
            strfile = "./";
        struct stat s;
        if (stat(strfile, &s) < 0)//文件不存在
        {
            printf("file not fount\n");
            send_header(info->fd, 404, "NOT FOUND", get_mime_type("*.html"), 0);
            send_file(info->fd, "error.html", info->epfd, 1);
        }
        else
        {
            if (S_ISREG(s.st_mode))//普通文件
            {
                printf("file\n");
                send_header(info->fd, 200, "OK", get_mime_type(strfile), s.st_size);
                send_file(info->fd, strfile, info->epfd, 1);

            }
            else if (S_ISDIR(s.st_mode))//目录
            {
                printf("dir\n");
                send_header(info->fd, 200, "OK", get_mime_type("*.html"), 0);
                send_file(info->fd, "dir_header.html", info->epfd, 0);

                struct dirent** mylist = NULL;
                char buf[N] = {0};
                int len = 0;
                int n = scandir(strfile, &mylist, NULL, alphasort);
                for (int i = 0; i < n; i++)
                {
                    if (mylist[i]->d_type == DT_DIR)//如果是目录
                    {
                        len = sprintf(buf, "<li><a href=%s/ >%s</a></li>", mylist[i]->d_name, mylist[i]->d_name);
                    }
                    else
                    {
                        len = sprintf(buf, "<li><a href=%s >%s</a></li>", mylist[i]->d_name, mylist[i]->d_name);
                    }

                    send(info->fd, buf, len, 0);

                    free(mylist[i]);
                }
                free(mylist);
                send_file(info->fd, "dir_tail.html", info->epfd, 1);
            }
        }
    }
}

