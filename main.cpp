#include <stdio.h>
#include <unistd.h>
#include "server.h"

int main(int argc, char* argv[])
{
    if (argc < 3) 
    {
        printf("./a.out port path\n");
        return -1;
    }
    signal(SIGPIPE, SIG_IGN);

    unsigned short port = atoi(argv[1]);
    chdir(argv[2]);
    WebServer* webserver = new WebServer(port);
    webserver->epollRun();
    delete webserver;
    return 0;
} 
