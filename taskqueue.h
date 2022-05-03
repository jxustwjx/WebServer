#pragma once
#include <queue>
#include <pthread.h>

using callback = void(*)(void*);

struct Task
{
    Task()
    {
        this->function = nullptr;
        this->arg = nullptr;
    }

    Task(callback function, void* arg)
    {
        this->function = function;
        this->arg = arg;
    }
    callback function;
    void* arg;
};

class TaskQueue
{
    public:
        TaskQueue();

        ~TaskQueue();

        // 添加任务
        void addTask(Task task);
        void addTask(callback function, void* arg);

        // 取出一个任务
        Task getTask();

        // 当前任务个数
        inline int gettaskNumber()
        {
            return m_taskQ.size();
        }

    private:
        std::queue<Task> m_taskQ;
        pthread_mutex_t m_mutex;
};
