#include "threadpool.h"

ThreadPool::ThreadPool(int minn, int maxn)
{
    do
    {
        taskQ = new TaskQueue;
        if (taskQ == nullptr)
        {
            printf("new taskQ fail...\n");
            break;
        }
        threadIDs = new pthread_t[maxn];
        if (threadIDs == nullptr)
        {
            printf("new threadIDs fail...\n");
            break;
        }

        memset(threadIDs, 0, sizeof(pthread_t) * maxn);
        minNum = minn;
        maxNum = maxn;
        busyNum = 0;
        aliveNum = minn;
        exitNum = 0;

        if (pthread_mutex_init(&mutexPool, NULL) != 0 ||
                pthread_cond_init(&notEmpty, NULL) != 0)
        {
            printf("init mutex or cond fail...\n");
            break;
        }

        //创建线程
        pthread_create(&managerID, NULL, manager, this);
        for (int i = 0; i < minn; i ++)
        {
            pthread_create(&threadIDs[i], NULL, worker, this);
        }

        //如果以上初始化都没有问题
        return;

    } while (false);

    //如果以上初始化有问题,释放资源
    if (taskQ) delete taskQ;
    if (threadIDs) delete threadIDs;

    pthread_mutex_destroy(&mutexPool);
    pthread_cond_destroy(&notEmpty);
}

ThreadPool::~ThreadPool()
{
    shutdown = true;
    printf("threadpool is close...\n");
    pthread_join(managerID, NULL);
    for (int i = 0; i < aliveNum; i ++)
    {
        pthread_cond_signal(&notEmpty);
    }

    if (taskQ) delete taskQ;
    if (threadIDs) delete threadIDs;

    pthread_mutex_destroy(&mutexPool);
    pthread_cond_destroy(&notEmpty);
}

void ThreadPool::addTask(Task task)
{
    if (shutdown) return;
    taskQ->addTask(task);
    pthread_cond_signal(&notEmpty);
}

int ThreadPool::getBusyNumber()
{
    pthread_mutex_lock(&mutexPool);
    int busyNum = this-> busyNum;
    pthread_mutex_unlock(&mutexPool);
    return busyNum;
}

int ThreadPool::getAliveNumber()
{
    pthread_mutex_lock(&mutexPool);
    int aliveNum = this-> aliveNum;
    pthread_mutex_unlock(&mutexPool);
    return aliveNum;
}

void* ThreadPool::worker(void* arg)
{
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    while (true)
    {
        pthread_mutex_lock(&pool->mutexPool);

        //当前任务队列为空相应减少线程数量
        while (pool->taskQ->gettaskNumber() == 0 && !pool->shutdown)
        {
            pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);
            if (pool->exitNum > 0)
            {
                pool->exitNum --;
                if (pool->aliveNum > pool->minNum)
                {
                    pool->aliveNum --;
                    pthread_mutex_unlock(&pool->mutexPool);
                    pool->threadExit();
                }
            }
        }

        //判断线程池是否关闭
        if (pool->shutdown)
        {
            pthread_mutex_unlock(&pool->mutexPool);
            pool->threadExit();
        }

        //取任务，用该线程完成任务
        Task task = pool->taskQ->getTask();
        pool->busyNum ++;
        pthread_mutex_unlock(&pool->mutexPool);

        task.function(task.arg);
        delete task.arg;
        task.arg = nullptr;

        //printf("thread %ld finish working...\n", pthread_self());

        pthread_mutex_lock(&pool->mutexPool);
        pool->busyNum --;
        pthread_mutex_unlock(&pool->mutexPool);
    }
    return nullptr;
}

void* ThreadPool::manager(void* arg)
{
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    while (!pool->shutdown)
    {
        pthread_mutex_lock(&pool->mutexPool);
        int queueSize = pool->taskQ->gettaskNumber();
        int busyNum = pool->busyNum;
        int aliveNum = pool->aliveNum;
        pthread_mutex_unlock(&pool->mutexPool);

        //添加线程
        if (aliveNum < pool->maxNum && queueSize > aliveNum - busyNum)
        {
            pthread_mutex_lock(&pool->mutexPool);
            int count = 0;
            for (int i = 0; i < pool->maxNum && count < NUMBER
                    && pool->aliveNum < pool->maxNum; i ++)
            {
                if (pool->threadIDs[i] == 0)
                {
                    pthread_create(&pool->threadIDs[i], NULL, worker, pool);
                    count ++;
                    pool->aliveNum ++;
                }
            }
            pthread_mutex_unlock(&pool->mutexPool);
        }

        //销毁线程
        if (busyNum * 2 < aliveNum && aliveNum > pool->minNum)
        {
            pthread_mutex_lock(&pool->mutexPool);
            pool->exitNum = NUMBER;
            pthread_mutex_unlock(&pool->mutexPool);

            for (int i = 0; i < NUMBER; i ++)
            {
                pthread_cond_signal(&pool->notEmpty);
            }
        }
        
        sleep(3);
    }
    return nullptr;
}

void ThreadPool::threadExit()
{
    pthread_t tid = pthread_self();
    //打印退出线程号
    for (int i = 0; i < maxNum; i ++)
    {
        if (threadIDs[i] == tid)
        {
            threadIDs[i] = 0;
            printf("threadExit: called %ld exiting...\n", tid);
            break;
        }
    }
    pthread_exit(NULL);
}
