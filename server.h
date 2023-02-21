#pragma once
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <ctype.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/sendfile.h>
#include "threadpool.h"

const int N = 4096;

typedef struct sockInfo
{
    int epfd;
    int fd;
}sockInfo;

class WebServer
{
public:
	WebServer(unsigned int port, int minn = 4, int maxn = 8);
	~WebServer();
	static void acceptClient(void* arg);
    static void recvHttpRequest(void* arg);
	int initListenFd();
	void epollRun();
private:
	unsigned int m_port;
	int epfd;
	int lfd;
	ThreadPool* pool;
};

void parseRequestLine(const char* buf, int cfd);
const char* get_mime_type(const char* name);
int hexit(char c);
void strencode(char* to, size_t tosize, const char* from);
void send_file(int cfd, const char* path);
void strdecode(char* to, char* from);
void send_header(int cfd, int code, const char* info, const char* filetype, int length);
