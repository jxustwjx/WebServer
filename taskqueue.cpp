#include "taskqueue.h"

TaskQueue::TaskQueue()
{
    pthread_mutex_init(&m_mutex, NULL);
}

TaskQueue::~TaskQueue()
{
    pthread_mutex_destroy(&m_mutex);
}

void TaskQueue::addTask(Task task)
{
    pthread_mutex_lock(&m_mutex);
    m_taskQ.push(task);
    pthread_mutex_unlock(&m_mutex);
}

void TaskQueue::addTask(callback function, void* arg)
{
    pthread_mutex_lock(&m_mutex);
    m_taskQ.push(Task(function, arg));
    pthread_mutex_unlock(&m_mutex);
}

Task TaskQueue::getTask()
{
    Task task;
    if (!m_taskQ.empty())
    {
        pthread_mutex_lock(&m_mutex);
        task = m_taskQ.front();
        m_taskQ.pop();
        pthread_mutex_unlock(&m_mutex);
    }
    return task;
}


