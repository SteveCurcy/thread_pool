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
#include <functional>
#include "Task.h"
#include "Queue.h"

/* 此处是各个类的声明，主要为了后续交叉引用做准备 */
class Thread;
// class ThreadManager;

class Thread final
{
public:
    using ThreadStatus = int;
    static constexpr int THREAD_CREATED = 0;
    static constexpr int THREAD_RUNNING = 1;
    static constexpr int THREAD_PAUSE = 2;
    static constexpr int THREAD_TERMINATED = -1;

private:
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

/**
 * ThreadManager 用于管理线程和任务，他将不同线程的任务申请按照既定的策略进行分配
 *
 * _M_coreSize 为最大核心线程数量；
 * _M_poolSize 为最大线程数量，包括核心线程；
 * _M_cores 为存储核心线程的集合
 * _M_threads 为存储普通/辅助线程的集合
 * _M_tasks 为任务队列，如果后期有动态变更队列类型的需求，则可以将其类型改为 Queue
 */
// class ThreadManager {
//     size_t _M_coreSize;
//     size_t _M_poolSize;
//     std::atomic<PoolStatus> _M_status;
//     /* 暂时只支持一个任务添加入口，因此对线程的操作也只有一个入口，
//      * 所以此处不考虑多线程安全问题，使用 deque 存储已有线程 */
//     std::unordered_map<std::thread::id, std::unique_ptr<Thread>> _M_cores;
//     std::unordered_map<std::thread::id, std::unique_ptr<Thread>> _M_threads;
//     DynamicQueue<Task> _M_tasks;
// public:
//     ThreadManager(size_t core = 3, size_t pool = 10): _M_coreSize(core), _M_poolSize(pool) { _M_status = 0; }

//     ~ThreadManager();

//     size_t size() const { return POOL_COUNT_MASK & _M_status.load(std::memory_order::memory_order_consume); }

//     bool addThread(const Strategy& strategy, bool isCore);

//     DynamicQueue<Task>& getTaskQue() { return _M_tasks; }

//     /* 从哈希表中移除一个线程 */
//     void removeThread(std::thread::id id);

//     /* 获取核心线程数量 */
//     size_t getCore() const { return _M_cores.size(); }

//     /**
//      * 提交一个任务，先将其封装为 Task 的形式，然后根据当前线程池的状态决定下一步的操作：
//      * - 如果没有到达核心线程的最大数量，则开启核心线程，并将任务推入任务队列；
//      * - 如果任务队列没满，且核心线程达到最大数量，则将任务直接放入任务队列；
//      * - 如果任务队列还差一个就满，且线程数量还没达到最大值，则开启一个新的普通线程，并将任务放入任务队列；
//      * - 如果任务队列还差一个满，且线程已经到达最大数量，则拒绝任务
//      *
//      * **NOTE**: packged_task 存储的目标可调用对象可能在**堆**上申请，或者使用提供的分配器。
//      */
//     template<typename F, typename ...ArgTp>
//     std::future<typename std::result_of<F(ArgTp)...>::type> submit(F &&f, ArgTp &&...args) {

//         using result_type = typename std::result_of<F(ArgTp)...>::type;
//         std::packaged_task<result_type()> task_ = std::packaged_task<result_type()>(
//                                                     std::bind(std::forward<F>(f),
//                                                     std::forward<ArgTp>(args)...));
//         std::future<result_type> res = task_.get_future();

//         Task task(std::move(task_));
//         std::future<result_type> dummy;

//         if (_M_cores.size() < _M_coreSize) {
//             if (!addThread(defaultStrategy, true)) return dummy;
//             if (!_M_tasks.push(&task)) return dummy;
//         } else if (_M_tasks.size() + 1 < _M_tasks.capacity()) {
//             if (!_M_tasks.push(&task)) return dummy;
//         } else if (_M_tasks.size() + 1 == _M_tasks.capacity() && _M_cores.size() + _M_threads.size() < _M_poolSize) {
//             if (!addThread(defaultStrategy, false)) return dummy;
//             if (!_M_tasks.push(&task)) return dummy;
//         } else {
//             return dummy;
//         }

//         return res;
//     }
// };

#endif
