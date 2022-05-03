#pragma once
#include "taskqueue.h"
#include <iostream>
#include <cstring>
#include <unistd.h>

class ThreadPool
{
    public:
        ThreadPool(int minn, int maxn);
        ~ThreadPool();
        
        void addTask(Task task);
        int getBusyNumber();
        int getAliveNumber();
        void threadExit();

    private:
        //创建线程要用对象函数的指针，对象不一定被创建，加static改为类函数
        static void* worker(void* arg);
        static void* manager(void* arg);

    private:
        TaskQueue* taskQ;

        pthread_t managerID;
        pthread_t* threadIDs;
        int minNum;
        int maxNum;
        int busyNum;
        int aliveNum;
        int exitNum;
        
        pthread_mutex_t mutexPool;
        pthread_cond_t notEmpty;
        static const int NUMBER = 2;

        bool shutdown = false;
};
