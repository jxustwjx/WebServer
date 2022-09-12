#include "server.h"

int initListenFd(unsigned short port)
{
    //�����׽���
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1)
    {
        perror("socket error");
        exit(1);
    }

    //��IP�Ͷ˿�
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    //inet_pton(AF_INET, "192.168.10.129", &server_addr.sin_addr.s_addr);

    //���ö˿ڸ���
    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    //��
    int ret = bind(lfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (ret == -1)
    {
        perror("bind error");
        exit(1);
    }

    //����
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

    //���÷Ƕ�������
    int flag = fcntl(cfd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);

    //���ü��Ӷ����������ش���
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = cfd;

    //���¼�����
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
    //��ȡ����(�ȶ�ȡһ��,�ڰ������ж�ȡ,�ӵ�)
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
        //����������
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
    //����������  GET /a.txt  HTTP/1.1\r\n
    char method[256] = {0};
    char content[256] = {0};
    sscanf(buf, "%[^ ] %[^ ]", method, content);
    printf("[%s]  [%s]\n", method, content);

    if (strcmp(method, "get") == 0 || strcmp(method, "GET") == 0)
    {
        char* strfile = content + 1;
        strdecode(strfile, strfile);  //����ת��
        if (*strfile == 0)//���û�������ļ�,Ĭ������ǰĿ¼
            strfile = "./";
        struct stat s;    //linux�й��ļ��Ľṹ�壬���Եõ��ļ�������
        if (stat(strfile, &s) < 0)//�ļ�������
        {
            printf("file not fount\n");
            send_header(cfd, 404, "NOT FOUND", get_mime_type("*.html"), 0);
            send_file(cfd, "error.html");
        }
        else
        {
            if (S_ISREG(s.st_mode))//��ͨ�ļ�
            {
                printf("request is a File\n");
                send_header(cfd, 200, "OK", get_mime_type(strfile), s.st_size);
                send_file(cfd, strfile);

            }
            else if (S_ISDIR(s.st_mode))//Ŀ¼
            {
                printf("request is Dir\n");
                send_header(cfd, 200, "OK", get_mime_type("*.html"), 0);
                send_file(cfd, "dir_header.html");

                struct dirent** mylist = NULL;
                char buf[N] = {0};
                int len = 0;
                int n = scandir(strfile, &mylist, NULL, alphasort); //��ȡĿ¼������ļ�
                for (int i = 0; i < n; i++)
                {
                    if (mylist[i]->d_type == DT_DIR)//�����Ŀ¼
                    {
                        len = sprintf(buf, "<li><a href=%s/ >%s</a></li>", mylist[i]->d_name, mylist[i]->d_name);
                    }
                    else  //������ļ�
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
    //����
    int epfd = epoll_create(1);
    if (epfd == -1)
    {
        perror("epoll_create error");
        exit(1);
    }

    //�ü������ļ�����������
    struct epoll_event ev;
    ev.events = EPOLLIN;     //���lfd�Ķ�������
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

//ͨ���ļ����ֻ���ļ�����
const char* get_mime_type(const char* name)
{
    const char* dot;

    dot = strrchr(name, '.');	//����������ҡ�.���ַ�;�粻���ڷ���NULL
    /*
     *charset=iso-8859-1	��ŷ�ı��룬˵����վ���õı�����Ӣ�ģ�
     *charset=gb2312		˵����վ���õı����Ǽ������ģ�
     *charset=utf-8			��������ͨ�õ����Ա��룻
     *						�����õ����ġ����ġ����ĵ��������������Ա�����
     *charset=euc-kr		˵����վ���õı����Ǻ��ģ�
     *charset=big5			˵����վ���õı����Ƿ������ģ�
     *
     *���������ݴ��ݽ������ļ�����ʹ�ú�׺�ж��Ǻ����ļ�����
     *����Ӧ���ļ����Ͱ���http����Ĺؼ��ַ��ͻ�ȥ
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

//����ĺ����ڶ���ʹ��
/*
 * ����������Ǵ���%20֮��Ķ�������"����"���̡�
 * %20 URL�����еġ� ��(space)
 * %21 '!' %22 '"' %23 '#' %24 '$'
 * %25 '%' %26 '&' %27 ''' %28 '('......
 * ���֪ʶhtml�еġ� ��(space)��&nbsp
 */
// %E8%8B%A6%E7%93%9C
void strdecode(char *to, char *from)
{
    for ( ; *from != '\0'; ++to, ++from) {

        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) { //�����ж�from�� %20 �����ַ�

            *to = hexit(from[1])*16 + hexit(from[2]);//�ַ���E8�����������16���Ƶ�E8
            from += 2;                      //�ƹ��Ѿ�����������ַ�(%21ָ��ָ��1),���ʽ3��++from�����������һ���ַ�
        } else
            *to = *from;
    }
    *to = '\0';
}

//16������ת��Ϊ10����, return 0�������
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

//"����"��������д�������ʱ�򣬽�����ĸ���ּ�/_.-~������ַ�ת����д��
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
    //����״̬��
    char buf[1024] = {0};
    int len = 0;
    len = sprintf(buf, "HTTP/1.1 %d %s\r\n", code, info);
    send(cfd, buf, len, 0);
    //������Ӧͷ
    len = sprintf(buf, "Content-Type:%s\r\n", filetype);
    send(cfd, buf, len, 0);
    if (length > 0)
    {
        len = sprintf(buf, "Content-Length:%d\r\n", length);
        send(cfd, buf, len, 0);

    }
    //����
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
            usleep(10); //���Ͷ˷���̫����Ի���һЩ����֮������⣬������һ��
        }
        memset(buf, 0, sizeof(buf));
    }

    close(fd);
}
