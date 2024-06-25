/**
 * @file Thread.h
 * @author Xu.Cao
 * @details
 *  本代码主要用于线程管理，由于线程管理部分耦合度较高，因此统一放置在本文件中。
 * 本模块主要包括：Thread、ThreadManager 两大类，具体细节
 * 请参见类注释。
 */
#ifndef THREAD_H
#define THREAD_H

#include <thread>
#include <atomic>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <functional>
#include "Task.h"
#include "Lock.h"
#include "Queue.h"

/* 此处是各个类的声明，主要为了后续交叉引用做准备 */
class Thread;
// class ThreadManager;

class Thread final
{
    using ThreadStatus = int;
    static constexpr int THREAD_CREATED = 0x1;
    static constexpr int THREAD_RUNNING = 0x2;
    static constexpr int THREAD_PAUSE = 0x4;
    static constexpr int THREAD_TERMINATED = 0x8;

    Queue<Task> *_M_taskQue;
    std::thread *_M_thread;
    std::atomic<ThreadStatus> _M_status;

    std::mutex _M_mutex;
    std::condition_variable _M_cond;

public:
    void run();

    ~Thread();

    Thread() : _M_taskQue(nullptr), _M_status(THREAD_CREATED) {}

    Thread(Queue<Task> *taskQueue) : _M_taskQue(taskQueue),
                                     _M_status(THREAD_CREATED)
    {
        _M_thread = new std::thread(&Thread::run, this);
    }

    Thread(const Thread &other) = delete;
    Thread(Thread &&other) noexcept = delete;
    Thread &operator=(const Thread &other) = delete;
    Thread &operator=(Thread &&other) noexcept = delete;

    void setQue(Queue<Task> *quePtr)
    {
        if (!_M_taskQue &&
            _M_status.load(std::memory_order_consume) & THREAD_CREATED)
        {
            _M_taskQue = quePtr;
        }

        _M_thread = new std::thread(&Thread::run, this);
    }

    void start();

    void pause();

    void resume();

    void shutdown();

    ThreadStatus getStatus() const { return _M_status.load(std::memory_order::memory_order_consume); }
};

class ThreadManager
{
    // 状态应该使用**位**存储，确保可以一次性判断是否处于某个状态集合。
    // 例如，是否正在运行或者已经停止，可以使用：
    // _M_status & (POOL_PAUSE | POOL_TERMINATED) 来测试
    using PoolStatus = int;
    static constexpr int POOL_CREATED = 0x1;
    static constexpr int POOL_RUNNING = 0x2;
    static constexpr int POOL_PAUSE = 0x4;
    static constexpr int POOL_TERMINATED = 0x8;

    static const size_t coreNr;

    std::vector<Thread> _M_threads;
    LockFreeQueue<Task> _M_tasks;
    std::atomic<PoolStatus> _M_status;
    size_t _M_poolSize;
    std::atomic<size_t> _M_activeNr;
    std::thread _M_manager;

    std::mutex _M_mutex;
    // 使用自旋锁将线程池状态和线程实体绑定，
    // 避免出现状态和线程实际工作状态不一致。
    // 改变状态和线程操作同时进行
    spinLock _M_threadLock;
    std::condition_variable _M_cond;

public:
    ThreadManager(size_t poolSize = 10, size_t queueSize = 1000)
        : _M_threads(poolSize), _M_tasks(queueSize),
          _M_status(POOL_CREATED), _M_poolSize(poolSize),
          _M_activeNr(0)
    {
        if (poolSize <= 1)
        {
            _M_poolSize = poolSize = 2;
        }
        for (int i = 0; i < poolSize; i++)
        {
            _M_threads[i].setQue(&_M_tasks);
        }
        _M_manager = std::thread(&ThreadManager::manage, this);

#ifndef NDEBUG
        printf("[INFO] ThreadPool: Initialized with %lu threads and %lu slots in the queue at most!\n",
               _M_poolSize, queueSize);
#endif
    }

    ~ThreadManager();

    void manage();

    void start();

    void pause();

    void resume();

    // 关闭，但是完成剩余任务
    void shutdown();

    // 关闭，并且抛弃剩余任务
    void forceShutdown();

    template <typename F, typename... ArgTp>
    std::future<typename std::result_of<F(ArgTp)...>::type> trySubmit(F &&f, ArgTp &&...args)
    {
        using result_type = typename std::result_of<F(ArgTp)...>::type;
        std::future<result_type> dummy;
        if (_M_tasks.full())
        {
#ifndef NDEBUG
            printf("\033[33m[WARNING] ThreadPool: Task queue is full and a task appended failed!\033[0m\n");
#endif
            return dummy;
        }

        std::packaged_task<result_type()> task_(std::bind(std::forward<F>(f),
                                                          std::forward<ArgTp>(args)...));
        std::future<result_type> res = task_.get_future();

        Task task(std::move(task_));

        if (_M_status.load(std::memory_order_consume) & (~POOL_RUNNING) ||
            !_M_tasks.push(&task, 1))
        {
#ifndef NDEBUG
            printf("\033[33m[WARNING] ThreadPool: Task appended failed, 'cause pool is not running!\033[0m\n");
#endif
            return dummy;
        }

        return res;
    }

    template <typename F, typename... ArgTp>
    std::future<typename std::result_of<F(ArgTp)...>::type> submit(F &&f, ArgTp &&...args)
    {
        using result_type = typename std::result_of<F(ArgTp)...>::type;

        std::packaged_task<result_type()> task_(std::bind(std::forward<F>(f),
                                                          std::forward<ArgTp>(args)...));
        std::future<result_type> res = task_.get_future();

        Task task(std::move(task_));

        // 在添加任务的时候，要确保线程池处于执行状态，且状态不可改变
        _M_threadLock.lock();
        if (_M_status.load(std::memory_order_consume) & POOL_RUNNING)
        {
            while (!_M_tasks.push(&task, 1))
            {
                std::this_thread::yield();
            }
        }
        _M_threadLock.unlock();

        return res;
    }
};

#endif
