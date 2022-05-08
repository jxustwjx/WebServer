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

char *get_mime_type(char *name);
int get_line(int sock, char *buf, int size);
int hexit(char c);//16����ת10����
void strencode(char* to, size_t tosize, const char* from);//����
void strdecode(char *to, char *from);//����
static ssize_t my_read(int fd, char *ptr);
ssize_t Readline(int fd, char *vptr, size_t maxlen);
void send_file(int cfd, char* path, int epfd, int flag);
void send_header(int cfd, int code, char* info, char* filetype, int length);

