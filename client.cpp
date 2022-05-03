#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

int main()
{
    //创建套接字
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
    {
        perror("socket");
        exit(0);
    }

    //连接服务器
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(10000);
    inet_pton(AF_INET, "192.168.10.129", &addr.sin_addr.s_addr);
    int ret = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret == -1)
    {
        perror("connect");
        exit(0);
    }

    int number = 0;
    //通信
    while (number <= 100)
    {
        char buf[1024];
        sprintf(buf, "there is client...%d\n", number++);
        write(fd, buf, strlen(buf));

        memset(buf, 0, sizeof(buf));
        int len = read(fd, buf, sizeof(buf));
        if (len == -1)
        {
            perror("read");
            break;
        }
        else if (len == 0)
        {
            printf("服务器断开连接:...\n");
            break;
        }
        else 
        {
            printf("Receive message: %s\n", buf);
        }
        usleep(100000);
    }
    return 0;
}
