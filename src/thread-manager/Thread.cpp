#include "Thread.h"

Thread::~Thread()
{
    shutdown();
    delete _M_thread;
}

void Thread::run()
{
    while (_M_status.load(std::memory_order_consume) &
           (~THREAD_TERMINATED))
    {
        switch (_M_status.load())
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
    if (expectStatus & (THREAD_PAUSE | THREAD_TERMINATED))
        return;
    _M_status.compare_exchange_strong(
        expectStatus, THREAD_PAUSE,
        std::memory_order_acq_rel);
}

void Thread::resume()
{
    int expectStatus = _M_status;
    if (expectStatus & (THREAD_RUNNING | THREAD_TERMINATED))
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

    if (expectStatus & THREAD_TERMINATED)
    {
        return;
    }
    if (_M_status.compare_exchange_strong(
            expectStatus, THREAD_TERMINATED,
            std::memory_order_acq_rel))
    {
        // 要确保首先转换状态，只完成正在执行的任务即可，
        // 如果暂停或者还没开始，则直接唤醒并结束即可
        if (expectStatus & (THREAD_CREATED | THREAD_PAUSE))
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

#ifndef NDEBUG
    printf("[INFO] Thread: Shutted.\n");
#endif
}

const size_t ThreadManager::coreNr = std::thread::hardware_concurrency();

ThreadManager::~ThreadManager()
{
    if (_M_status.load(std::memory_order_consume) &
        (~POOL_TERMINATED))
    {
        shutdown();
    }
}

void ThreadManager::manage()
{
    // 管理工作线程
    while (_M_status.load(std::memory_order_consume) &
           (~POOL_TERMINATED))
    {
        switch (_M_status.load())
        {
        case POOL_RUNNING:
        {
            // 当线程池正在运行时，不断查看当前队列的压力，
            // 当压力增大时，增加活动工作线程的数量，否则减少，
            // 正在运行的线程数量不能少于CPU核心数
            _M_threadLock.lock();
            // 由于 vector 不能保证线程安全，因此操作时，
            // 应该首先加锁，并且需要保证当前线程池不是
            // 暂停、终止或开始
            if (_M_status.load(std::memory_order_consume) &
                POOL_RUNNING)
            {
                size_t expectNr = (int)_M_tasks.getStress() * _M_poolSize;
                size_t nowNr = _M_activeNr;
                if (expectNr >= 2 && nowNr != expectNr &&
                    _M_activeNr.compare_exchange_strong(
                        nowNr, expectNr,
                        std::memory_order_acq_rel))
                {
                    if (nowNr > expectNr)
                    {
                        for (int i = nowNr - 1;
                             i >= expectNr - 1; i--)
                        {
                            _M_threads[i].pause();
                        }
                    }
                    else
                    {
                        for (int i = nowNr - 1; i < expectNr; i++)
                        {
                            _M_threads[i].resume();
                        }
                    }
                }
            }
            _M_threadLock.unlock();

            std::this_thread::yield();
        }
        break;
        case POOL_CREATED:
        case POOL_PAUSE:
        {
            std::unique_lock<std::mutex> lock(_M_mutex);
            _M_cond.wait(lock);
        }
        break;
        }
    }
#ifndef NDEBUG
    printf("[INFO] ThreadPool: Manager of pool ended!\n");
#endif
}

void ThreadManager::start()
{
    int expectStatus = POOL_CREATED;
    _M_threadLock.lock();
    if (_M_status.compare_exchange_strong(
            expectStatus, POOL_RUNNING,
            std::memory_order_acq_rel))
    {
        // 只有从 CREATED 状态到 RUNNING 状态
        // 才允许真正开始工作
        _M_activeNr.store(std::min(std::max((size_t)2, coreNr), _M_poolSize),
                          std::memory_order_release);
        for (int i = 0; i < _M_activeNr; i++)
        {
            _M_threads[i].start();
        }
    }
    _M_cond.notify_one();
    _M_threadLock.unlock();
#ifndef NDEBUG
    printf("[INFO] ThreadPool: %lu thread(s) and the Manager has started!\n", _M_activeNr.load(std::memory_order_relaxed));
#endif
}

void ThreadManager::pause()
{
    int expectStatus = _M_status;
    if (expectStatus & (POOL_PAUSE | POOL_TERMINATED))
        return;

    _M_threadLock.lock();
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
    _M_threadLock.unlock();
}

void ThreadManager::resume()
{
    int expectStatus = POOL_PAUSE;

    _M_threadLock.lock();
    if (_M_status.compare_exchange_strong(
            expectStatus, POOL_RUNNING,
            std::memory_order::memory_order_acq_rel))
    {
        for (int i = 0; i < _M_activeNr; i++)
        {
            _M_threads[i].resume();
        }
    }
    _M_threadLock.unlock();
}

void ThreadManager::shutdown()
{
    // 在全部强制关闭前，首先确保队列为空
    while (!_M_tasks.empty())
    {
        std::this_thread::yield();
    }

#ifndef NDEBUG
    printf("[INFO] ThreadPool: All tasks finished. Pool is shutting...\n");
#endif

    // 已经将所有队列的任务都拿出，直接结束即可
    forceShutdown();
}

void ThreadManager::forceShutdown()
{
    PoolStatus expectStatus = _M_status.load(std::memory_order_consume);
    if (expectStatus & POOL_TERMINATED)
        return;

#ifndef NDEBUG
    printf("[INFO] ThreadPool: Waiting the spin lock for shutting...\n");
#endif

    _M_threadLock.lock();

#ifndef NDEBUG
    printf("[INFO] ThreadPool: Start shutting the thread pool...\n");
#endif

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

#ifndef NDEBUG
    printf("[INFO] ThreadPool: All threads is shutted!\n");
#endif
    _M_threadLock.unlock();
    _M_manager.join();

#ifndef NDEBUG
    printf("[INFO] ThreadPool: Shutted!\n");
#endif
}