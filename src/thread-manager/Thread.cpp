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
    int expectStatus = _M_status.load();
    if (expectStatus == THREAD_RUNNING)
        return;
    _M_status.compare_exchange_strong(
        expectStatus, THREAD_PAUSE,
        std::memory_order_acq_rel);
}

void Thread::resume()
{
    int expectStatus = THREAD_PAUSE;
    if (_M_status.compare_exchange_strong(
            expectStatus, THREAD_RUNNING,
            std::memory_order_acq_rel))
    {
        // 只有从 CREATED 状态到 RUNNING 状态
        // 才允许真正开始工作
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