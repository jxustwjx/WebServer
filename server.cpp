#include "server.h"

int initListenFd(unsigned short port)
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
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    //inet_pton(AF_INET, "192.168.10.129", &server_addr.sin_addr.s_addr);

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
    return lfd;
}

void acceptClient(void* arg)
{
    printf("accept a client...\n");
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
    int ret = epoll_ctl(info->epfd, EPOLL_CTL_ADD, cfd, &ev);
    if (ret == -1)
    {
        perror("epoll_ctl");
    }
}

void recvHttpRequest(void* arg)
{
    printf("start recv message...\n");
    sockInfo* info = (sockInfo*)arg;
    //读取请求(先读取一行,在把其他行读取,扔掉)
    char buf[N] = {0};
    char tem[N] = {0};
    int len = 0, totle = 0;
    while ((len = recv(info->fd, tem, sizeof tem, 0)) > 0)
    {
        if (totle + len < sizeof buf)
        {
            memcpy(buf + totle, tem, len);
        }
        totle += len;
    }

    if (len == 0)
    {
        printf("Client is disconnect\n");
        epoll_ctl(info->epfd, EPOLL_CTL_DEL, info->fd, NULL);
        close(info->fd);
    }
    else if (len == -1 && errno == EAGAIN)
    {
        //解析请求行
        char* pos = strstr(buf, "\r\n");
        int len = pos - buf;
        buf[len] = 0;
        parseRequestLine(buf, info->fd);
    }
    else
    {
        perror("read error");
    }
}

void parseRequestLine(const char* buf, int cfd)
{
    //解析请求行  GET /a.txt  HTTP/1.1\r\n
    char method[256] = {0};
    char content[256] = {0};
    sscanf(buf, "%[^ ] %[^ ]", method, content);
    printf("[%s]  [%s]\n", method, content);

    if (strcmp(method, "get") == 0 || strcmp(method, "GET") == 0)
    {
        char* strfile = content + 1;
        strdecode(strfile, strfile);  //中文转换
        if (*strfile == 0)//如果没有请求文件,默认请求当前目录
            strfile = "./";
        struct stat s;    //linux有关文件的结构体，可以得到文件的属性
        if (stat(strfile, &s) < 0)//文件不存在
        {
            printf("file not fount\n");
            send_header(cfd, 404, "NOT FOUND", get_mime_type("*.html"), 0);
            send_file(cfd, "error.html");
        }
        else
        {
            if (S_ISREG(s.st_mode))//普通文件
            {
                printf("request is a File\n");
                send_header(cfd, 200, "OK", get_mime_type(strfile), s.st_size);
                send_file(cfd, strfile);

            }
            else if (S_ISDIR(s.st_mode))//目录
            {
                printf("request is Dir\n");
                send_header(cfd, 200, "OK", get_mime_type("*.html"), 0);
                send_file(cfd, "dir_header.html");

                struct dirent** mylist = NULL;
                char buf[N] = {0};
                int len = 0;
                int n = scandir(strfile, &mylist, NULL, alphasort); //获取目录里面的文件
                for (int i = 0; i < n; i++)
                {
                    if (mylist[i]->d_type == DT_DIR)//如果是目录
                    {
                        len = sprintf(buf, "<li><a href=%s/ >%s</a></li>", mylist[i]->d_name, mylist[i]->d_name);
                    }
                    else  //如果是文件
                    {
                        len = sprintf(buf, "<li><a href=%s >%s</a></li>", mylist[i]->d_name, mylist[i]->d_name);
                    }

                    send(cfd, buf, len, 0);

                    free(mylist[i]);
                }
                free(mylist);
                send_file(cfd, "dir_tail.html");
            }
        }
    }
}

void epollRun(int lfd)
{
    printf("epollRun...\n");
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
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
    if (ret == -1)
    {
        perror("epoll_ctl error");
        exit(1);
    }

	ThreadPool* pool = new ThreadPool(4, 8);

    struct epoll_event evs[N];
    int size = sizeof(evs) / sizeof(struct epoll_event);

    while (true)
    {
        int num = epoll_wait(epfd, evs, size, -1);
        //printf("%d fd is change...\n", num);
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
                printf("lfd\n");
                pool->addTask(Task(acceptClient, info));
            }
            else
            {
                printf("cfd\n");
                pool->addTask(Task(recvHttpRequest, info));
            }
        }
    }
}

//通过文件名字获得文件类型
const char* get_mime_type(const char* name)
{
    const char* dot;

    dot = strrchr(name, '.');	//自右向左查找‘.’字符;如不存在返回NULL
    /*
     *charset=iso-8859-1	西欧的编码，说明网站采用的编码是英文；
     *charset=gb2312		说明网站采用的编码是简体中文；
     *charset=utf-8			代表世界通用的语言编码；
     *						可以用到中文、韩文、日文等世界上所有语言编码上
     *charset=euc-kr		说明网站采用的编码是韩文；
     *charset=big5			说明网站采用的编码是繁体中文；
     *
     *以下是依据传递进来的文件名，使用后缀判断是何种文件类型
     *将对应的文件类型按照http定义的关键字发送回去
     */
    if (dot == (char*)0)
        return "text/plain; charset=utf-8";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp( dot, ".wav") == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";

    return "text/plain; charset=utf-8";
}
/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/

//下面的函数第二天使用
/*
 * 这里的内容是处理%20之类的东西！是"解码"过程。
 * %20 URL编码中的‘ ’(space)
 * %21 '!' %22 '"' %23 '#' %24 '$'
 * %25 '%' %26 '&' %27 ''' %28 '('......
 * 相关知识html中的‘ ’(space)是&nbsp
 */
// %E8%8B%A6%E7%93%9C
void strdecode(char *to, char *from)
{
    for ( ; *from != '\0'; ++to, ++from) {

        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) { //依次判断from中 %20 三个字符

            *to = hexit(from[1])*16 + hexit(from[2]);//字符串E8变成了真正的16进制的E8
            from += 2;                      //移过已经处理的两个字符(%21指针指向1),表达式3的++from还会再向后移一个字符
        } else
            *to = *from;
    }
    *to = '\0';
}

//16进制数转化为10进制, return 0不会出现
int hexit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return 0;
}

//"编码"，用作回写浏览器的时候，将除字母数字及/_.-~以外的字符转义后回写。
//strencode(encoded_name, sizeof(encoded_name), name);
void strencode(char* to, size_t tosize, const char* from)
{
    int tolen;

    for (tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from) {
        if (isalnum(*from) || strchr("/_.-~", *from) != (char*)0) {
            *to = *from;
            ++to;
            ++tolen;
        } else {
            sprintf(to, "%%%02x", (int) *from & 0xff);
            to += 3;
            tolen += 3;
        }
    }
    *to = '\0';
}


void send_header(int cfd, int code, const char* info, const char* filetype, int length)
{
    //发送状态行
    char buf[1024] = {0};
    int len = 0;
    len = sprintf(buf, "HTTP/1.1 %d %s\r\n", code, info);
    send(cfd, buf, len, 0);
    //发送响应头
    len = sprintf(buf, "Content-Type:%s\r\n", filetype);
    send(cfd, buf, len, 0);
    if (length > 0)
    {
        len = sprintf(buf, "Content-Length:%d\r\n", length);
        send(cfd, buf, len, 0);

    }
    //空行
    send(cfd, "\r\n", 2, 0);
}

void send_file(int cfd, const char* path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        perror("open error");
        return;
    }

    char buf[1024] = {0};
    while (1)
    {
        int len = read(fd, buf, sizeof(buf));
        if (len <= 0)
        {
            break;
        }
        else
        {
            send(cfd, buf, len, 0);
            usleep(10); //发送端发送太快可以会有一些意料之外的问题，让它慢一点
        }
        memset(buf, 0, sizeof(buf));
    }

    close(fd);
}
