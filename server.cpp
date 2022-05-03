#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include "threadpool.h"

void check(void* arg);
void acceptConn(void* arg);
void communication(void* arg);

typedef struct PoolInfo
{
    ThreadPool* pool;
    int epfd;
    int fd;
}PoolInfo;

typedef struct EpollInfo
{
    int epfd;
    int fd;
}EpollInfo;

int main()
{
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
    server_addr.sin_port = htons(10000);
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
    ThreadPool* pool = new ThreadPool(10, 100);

    PoolInfo* pinfo = new PoolInfo;
    pinfo->pool = pool;
    pinfo->epfd = epfd;
    pinfo->fd = lfd;

    pool->addTask(Task(check, pinfo));
    pthread_exit(NULL);
    return 0;
}

void check(void* arg)
{
    PoolInfo* pinfo = (PoolInfo*)arg;
    struct epoll_event evs[1024];
    int size = sizeof(evs) / sizeof(struct epoll_event);
    //持续检测
    while (true)
    {
        int num = epoll_wait(pinfo->epfd, evs, size, -1);
        printf("%d fd is change...\n", num);
        EpollInfo* info = new EpollInfo;
        info->epfd = pinfo->epfd;

        for (int i = 0; i < num; i ++)
        {
            int fd = evs[i].data.fd;
            if (fd == pinfo->fd)
            {
                info->fd = fd;
                pinfo->pool->addTask(Task(acceptConn, info));
            }
            else
            {
                info->fd = fd;
                pinfo->pool->addTask(Task(communication, info));
            }
        }
        sleep(1);
    }
}

void acceptConn(void* arg)  
{
    //epoll树会自动检测文件描述符的变化，不需要再while(true)
    EpollInfo* info = (EpollInfo*)arg;
    int cfd = accept(info->fd, NULL, NULL);
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
    EpollInfo* info = (EpollInfo*)arg;
    int cfd = info->fd;
    int epfd = info->epfd;
    char buf[1024];

    while (true)
    {
        usleep(100000);
        int len = read(cfd, buf, sizeof(buf));
        if (len == -1)
        {
            if (errno == EAGAIN)
            {
                if (strlen(buf) != 0)
                {
                    write(cfd, buf, strlen(buf));
                    memset(buf, 0, sizeof(buf));
                }
            }
            else 
            {
                perror("read error");
                break;
            }
        }
        else if (len == 0)
        {
            printf("客户端已断开连接...\n");
            epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
            close(cfd);
            break;
        }
        else
        {
            printf("Receive Message: %s\n", buf);
            for (int i = 0; i < len; i ++)
            {
                buf[i] = toupper(buf[i]);
            }
        }
    }
}
