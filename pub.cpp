#include "pub.h"

//ͨ���ļ����ֻ���ļ�����
char *get_mime_type(char *name)
{
    char* dot;

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
//���һ�����ݣ�ÿ����\r\n��Ϊ�������
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        /* DEBUG printf("%02X\n", c); */
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);//MSG_PEEK �ӻ����������ݣ��������ݲ��ӻ��������
                /* DEBUG printf("%02X\n", c); */
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    return(i);
}

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

static ssize_t my_read(int fd, char *ptr)
{
    static int read_cnt;
    static char *read_ptr;
    static char read_buf[100];

    if (read_cnt <= 0) {
again:
        if ( (read_cnt = read(fd, read_buf, sizeof(read_buf))) < 0) {
            if (errno == EINTR)
                goto again;
            return -1;
        } else if (read_cnt == 0)
            return 0;
        read_ptr = read_buf;
    }
    read_cnt--;
    *ptr = *read_ptr++;

    return 1;
}

ssize_t Readline(int fd, char* vptr, size_t maxlen)
{
    ssize_t n, rc;
    char c, *ptr;

    ptr = vptr;
    for (n = 1; n < maxlen; n++) {
        if ( (rc = my_read(fd, &c)) == 1) {
            if (c  == '\n')
                break;
            *ptr++ = c;
        } else if (rc == 0) {
            *ptr = 0;
            return n - 1;
        } else
            return -1;
    }
    *ptr  = 0;

    return n;
}

void send_header(int cfd, int code, char* info, char* filetype, int length)
{	//����״̬��
    char buf[1024] = {0};
    int len = 0;
    len = sprintf(buf, "HTTP/1.1 %d %s\r\n", code, info);
    send(cfd, buf, len, 0);
    //������Ϣͷ
    len = sprintf(buf, "Content-Type:%s\r\n", filetype);
    send(cfd, buf, len, 0);
    if (length > 0)
    {
        //������Ϣͷ
        len = sprintf(buf, "Content-Length:%d\r\n", length);
        send(cfd, buf, len, 0);

    }
    //����
    send(cfd, "\r\n", 2, 0);
}

void send_file(int cfd, char* path, int epfd, int flag)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        perror("open");
        return;
    }
    char buf[1024] = {0};
    int len = 0;
    while (1)
    {

        len = read(fd, buf, sizeof(buf));
        if (len < 0)
        {
            perror("read");
            break;

        }
        else if (len == 0)
        {
            break;
        }
        else
        {
            int n = 0;
            n = send(cfd, buf, len, 0);
            printf("len=%d\n", n);

        }
    }
    close(fd);
    //�ر�cfd,����
    if (flag == 1)
    {
        close(cfd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
    }
}
