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
#include "Queue.h"

/* 此处是各个类的声明，主要为了后续交叉引用做准备 */
class Thread;
// class ThreadManager;

class Thread final
{
    using ThreadStatus = int;
    static constexpr int THREAD_CREATED = 0;
    static constexpr int THREAD_RUNNING = 1;
    static constexpr int THREAD_PAUSE = 2;
    static constexpr int THREAD_TERMINATED = -1;

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
            _M_status.load(std::memory_order_consume) == THREAD_CREATED)
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
    using PoolStatus = int;
    static constexpr int POOL_CREATED = 0;
    static constexpr int POOL_RUNNING = 1;
    static constexpr int POOL_PAUSE = 2;
    static constexpr int POOL_TERMINATED = -1;

    static const int coreNr;

    std::vector<Thread> _M_threads;
    LockFreeQueue<Task> _M_tasks;
    std::atomic<PoolStatus> _M_status;
    size_t _M_poolSize;
    std::atomic<size_t> _M_activeNr;

public:
    ThreadManager(size_t poolSize = 10, size_t queueSize = 1000)
        : _M_threads(poolSize), _M_tasks(queueSize),
          _M_status(POOL_CREATED), _M_poolSize(poolSize),
          _M_activeNr(0)
    {
        for (int i = 0; i < poolSize; i++)
        {
            _M_threads[i].setQue(&_M_tasks);
        }
    }

    ~ThreadManager();

    void start();

    void pause();

    void resume();

    // 关闭，但是完成剩余任务
    void shutdown();

    // 关闭，并且抛弃剩余任务
    void forceShutdown();

    /**
     * 提交一个任务，先将其封装为 Task 的形式，然后根据当前线程池的状态决定下一步的操作：
     * - 如果没有到达核心线程的最大数量，则开启核心线程，并将任务推入任务队列；
     * - 如果任务队列没满，且核心线程达到最大数量，则将任务直接放入任务队列；
     * - 如果任务队列还差一个就满，且线程数量还没达到最大值，则开启一个新的普通线程，并将任务放入任务队列；
     * - 如果任务队列还差一个满，且线程已经到达最大数量，则拒绝任务
     *
     * **NOTE**: packged_task 存储的目标可调用对象可能在**堆**上申请，或者使用提供的分配器。
     */
    template <typename F, typename... ArgTp>
    std::future<typename std::result_of<F(ArgTp)...>::type> submit(F &&f, ArgTp &&...args)
    {

        using result_type = typename std::result_of<F(ArgTp)...>::type;
        std::packaged_task<result_type()> task_ = std::packaged_task<result_type()>(
            std::bind(std::forward<F>(f),
                      std::forward<ArgTp>(args)...));
        std::future<result_type> res = task_.get_future(), dummy;

        Task task(std::move(task_));

        if (_M_status.load(std::memory_order_consume) != POOL_RUNNING ||
            !_M_tasks.push(&task, 1))
        {
            return dummy;
        }

        return res;
    }
};

#endif
