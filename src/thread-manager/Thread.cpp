#include "Thread.h"
#include <stdio.h>

Thread::~Thread()
{
    shutdown();
    delete _M_thread;
    printf("[INFO] Thread deleted!\n");
}

void Thread::run()
{
    while (_M_status.load() != THREAD_TERMINATED)
    {
        switch (_M_status.load(std::memory_order_consume))
        {
        case THREAD_RUNNING:
        { // 大括号保证代码中使用的全部是局部变量
            // 当线程的状态为运行时，从队列中获取一个任务执行
            Task task;
            if (_M_taskQue->pop(&task, 1))
            {
                task();
            }
            else
            {
                std::this_thread::yield();
            }
        }
        break;
        case THREAD_CREATED:
        case THREAD_PAUSE:
        {
            std::unique_lock<std::mutex> lock(_M_mutex);
            _M_cond.wait(lock);
        }
        break;
        }
    }
}

void Thread::start()
{
    int expectStatus = THREAD_CREATED;
    if (_M_status.compare_exchange_strong(
            expectStatus, THREAD_RUNNING,
            std::memory_order_acq_rel))
    {
        // 只有从 CREATED 状态到 RUNNING 状态
        // 才允许真正开始工作
        _M_cond.notify_one();
    }
}

void Thread::pause()
{
    int expectStatus = _M_status;
    if (expectStatus == THREAD_PAUSE ||
        expectStatus == THREAD_TERMINATED)
        return;
    _M_status.compare_exchange_strong(
        expectStatus, THREAD_PAUSE,
        std::memory_order_acq_rel);
}

void Thread::resume()
{
    int expectStatus = _M_status;
    if (expectStatus != THREAD_PAUSE ||
        expectStatus != THREAD_CREATED)
        return;
    if (_M_status.compare_exchange_strong(
            expectStatus, THREAD_RUNNING,
            std::memory_order_acq_rel))
    {
        _M_cond.notify_one();
    }
}

void Thread::shutdown()
{
    int expectStatus = _M_status.load(std::memory_order_consume);

    if (expectStatus == THREAD_TERMINATED)
    {
        return;
    }
    if (_M_status.compare_exchange_strong(
            expectStatus, THREAD_TERMINATED,
            std::memory_order_acq_rel))
    {
        // 要确保首先转换状态，只完成正在执行的任务即可，
        // 如果暂停或者还没开始，则直接唤醒并结束即可
        if (expectStatus == THREAD_CREATED ||
            expectStatus == THREAD_PAUSE)
        {
            _M_cond.notify_one();
        }
        // 只有当线程状态成功被转为终止态,
        // 并且线程可以被终止才实现回收
        if (_M_thread->joinable())
        {
            _M_thread->join();
        }
    }
}

const int ThreadManager::coreNr = std::thread::hardware_concurrency();

ThreadManager::~ThreadManager()
{
    shutdown();
}

void ThreadManager::start()
{
    int expectStatus = POOL_CREATED;
    if (_M_status.compare_exchange_strong(
            expectStatus, POOL_RUNNING,
            std::memory_order_acq_rel))
    {
        // 只有从 CREATED 状态到 RUNNING 状态
        // 才允许真正开始工作
        _M_activeNr.store(std::max(2, coreNr),
                          std::memory_order_release);
        for (int i = 0; i < _M_activeNr; i++)
        {
            _M_threads[i].start();
        }
    }
}

void ThreadManager::pause()
{
    int expectStatus = _M_status;
    if (expectStatus == POOL_PAUSE ||
        expectStatus == POOL_TERMINATED)
        return;
    if (_M_status.compare_exchange_strong(
            expectStatus, POOL_PAUSE,
            std::memory_order_acq_rel))
    {
        // 如果状态更新成功，则将所有线程都暂停
        for (int i = 0; i < _M_activeNr; i++)
        {
            _M_threads[i].pause();
        }
    }
}

void ThreadManager::resume()
{
    int expectStatus = POOL_PAUSE;
    if (_M_status.compare_exchange_strong(
            expectStatus, POOL_RUNNING,
            std::memory_order_acq_rel))
    {
        for (int i = 0; i < _M_activeNr; i++)
        {
            _M_threads[i].resume();
        }
    }
}

void ThreadManager::shutdown()
{
    // 在全部强制关闭前，首先确保队列为空
    while (!_M_tasks.empty())
    {
        std::this_thread::yield();
    }

    // 已经将所有队列的任务都拿出，直接结束即可
    forceShutdown();
}

void ThreadManager::forceShutdown()
{
    PoolStatus expectStatus = _M_status.load(std::memory_order_consume);
    if (expectStatus == POOL_TERMINATED)
    {
        return;
    }
    if (_M_status.compare_exchange_strong(
            expectStatus, POOL_TERMINATED,
            std::memory_order_acq_rel))
    {
        // 要确保首先转换状态，直接全部结束即可
        for (int i = 0; i < _M_poolSize; i++)
        {
            _M_threads[i].shutdown();
        }
    }
}