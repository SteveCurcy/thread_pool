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
#include <functional>
#include <unordered_map>
#include "Task.h"
#include "Queue.h"

/* 此处是各个类的声明，主要为了后续交叉引用做准备 */
class Thread;
class ThreadManager;

/* 此处定义一些相关的类型，便于后续使用，将使用 using 而非 typedef */
using ThreadStatus = unsigned int;
using PoolStatus = unsigned int;   // status << 28 | count
/* Strategy 用于提供一个线程获取和执行任务的接口，策略应该作为线程和任务的粘合剂；
 * 进而实现线程管理和任务管理的解耦合，根据当前的任务数量、线程状态和线程池状态判断如何决策 */
using Strategy = std::function<size_t(const Thread&, ThreadManager&)>;

/* 线程正在处于的状态 */
constexpr int THREAD_STATUS_SIZE = 32;
constexpr int THREAD_STATUS_OFFSET = 28;
constexpr int THREAD_STATUS_CORE_OFF = 31;
constexpr ThreadStatus THREAD_CORE_MASK = ((1 << (THREAD_STATUS_SIZE - THREAD_STATUS_CORE_OFF)) - 1) << THREAD_STATUS_CORE_OFF;
constexpr ThreadStatus THREAD_STATUS_MASK = ((1 << (THREAD_STATUS_CORE_OFF - THREAD_STATUS_OFFSET)) - 1) << THREAD_STATUS_OFFSET;
constexpr ThreadStatus THREAD_IDLE_TIME_MASK = (1 << THREAD_STATUS_OFFSET) - 1;
constexpr ThreadStatus STAT_SHUTDOWN = 0;   /* 线程执行已获取的剩余的任务，即将退出 */
constexpr ThreadStatus STAT_RUN      = 1 << THREAD_STATUS_OFFSET;   /* 线程只在执行 */
/* 线程池正在处于的状态 */
constexpr int POOL_STATUS_SIZE   = 32;
constexpr int POOL_STATUS_OFFSET = 28;
constexpr PoolStatus POOL_STATUS_MASK = ((1 << (POOL_STATUS_SIZE - POOL_STATUS_OFFSET)) - 1) << POOL_STATUS_OFFSET;
constexpr PoolStatus POOL_COUNT_MASK = (1 << POOL_STATUS_OFFSET) - 1;
/* 线程最大空闲轮数 */
constexpr int MAX_IDLE_TIME = 10;
/* 核心线程最大支撑的任务数量 */
constexpr int MAX_CORE_LOAD = 5;

/* 策略的函数声明 */
size_t defaultStrategy(const Thread& thrd, ThreadManager& thrdMngr);

/**
 * Thread 类是对 thread 类的包装，用于更方便的线程控制。
 * _M_affiliation 是当前线程所属的线程管理器的指针，
 * _M_status 用于表示当前线程的状态信息，31 bit 表示是否为核心线程；
 *                                  30-28 bit 表示当前线程的运行状态；
 *                                  27-0 bit 表示线程允许的最大空闲次数
 * _M_thread 为实际的线程
 * execStrategy 为执行策略，根据当前线程状态和线程池状态来进行任务分配和执行
 * 
 * 线程开启后，不断执行既定的策略，由策略确定能否从队列中获取任务并进行执行；如果
 * 线程多轮没有获取到任务执行，则进行回收。
 * Thread 类会在最终析构函数中告知线程池，将其信息从保存的线程集合中移除
 * 
 * @fix **后期的执行策略应该放到线程管理器中，方便通过线程管理器来进行管理，
 *      线程管理器应该提供一个接口，用于获取任务或完成任务获取和执行**
 */
class Thread final {
    ThreadManager                *_M_affiliation;
    std::atomic<ThreadStatus>    _M_status;
    std::unique_ptr<std::thread> _M_thread;
    Strategy                     execStrategy;
public:
    void run();

    Thread() = default;

    /**
     * @param affiliation 从属的线程管理者
     * @param exec 线程的执行策略
     * @param core 是否为核心线程
     */
    Thread(ThreadManager *affiliation,
           const Strategy& exec, bool core):
            _M_affiliation(affiliation),
            _M_status(((unsigned int)core << THREAD_STATUS_CORE_OFF) | STAT_RUN | MAX_IDLE_TIME),
            execStrategy(exec) {
        _M_thread = std::unique_ptr<std::thread>(new std::thread(&Thread::run, this));
    }

    ~Thread();

    Thread(const Thread& other) = delete;
    Thread(Thread&& other) noexcept = delete;
    Thread& operator=(const Thread& other) = delete;
    Thread& operator=(Thread&& other) noexcept = delete;

    void shutdown() {
        auto currentState = _M_status.load(std::memory_order::memory_order_consume);
        _M_status.store(currentState & (~THREAD_STATUS_MASK), std::memory_order::memory_order_release);
        if (_M_thread->joinable())
            _M_thread->join();
    }

    ThreadStatus getStatus() const { return _M_status.load(std::memory_order::memory_order_consume) & THREAD_STATUS_MASK; }
    size_t getIdleTime() const { return _M_status.load(std::memory_order::memory_order_consume) & THREAD_IDLE_TIME_MASK; }
    bool isCore() const { return _M_status.load(std::memory_order::memory_order_consume) & THREAD_CORE_MASK; }
    void setStrategy(const Strategy& s) { execStrategy = s; }
    std::thread::id getId() const { return _M_thread->get_id(); }
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
class ThreadManager {
    size_t _M_coreSize;
    size_t _M_poolSize;
    std::atomic<PoolStatus> _M_status;
    /* 暂时只支持一个任务添加入口，因此对线程的操作也只有一个入口，
     * 所以此处不考虑多线程安全问题，使用 deque 存储已有线程 */
    std::unordered_map<std::thread::id, std::unique_ptr<Thread>> _M_cores;
    std::unordered_map<std::thread::id, std::unique_ptr<Thread>> _M_threads;
    DynamicQueue<Task> _M_tasks;
public:
    ThreadManager(size_t core = 3, size_t pool = 10): _M_coreSize(core), _M_poolSize(pool) { _M_status = 0; }

    ~ThreadManager();

    size_t size() const { return POOL_COUNT_MASK & _M_status.load(std::memory_order::memory_order_consume); }

    bool addThread(const Strategy& strategy, bool isCore);

    DynamicQueue<Task>& getTaskQue() { return _M_tasks; }

    /* 从哈希表中移除一个线程 */
    void removeThread(std::thread::id id);

    /* 获取核心线程数量 */
    size_t getCore() const { return _M_cores.size(); }

    /**
     * 提交一个任务，先将其封装为 Task 的形式，然后根据当前线程池的状态决定下一步的操作：
     * - 如果没有到达核心线程的最大数量，则开启核心线程，并将任务推入任务队列；
     * - 如果任务队列没满，且核心线程达到最大数量，则将任务直接放入任务队列；
     * - 如果任务队列还差一个就满，且线程数量还没达到最大值，则开启一个新的普通线程，并将任务放入任务队列；
     * - 如果任务队列还差一个满，且线程已经到达最大数量，则拒绝任务
     */
    template<typename F, typename... Args>
    std::future<typename std::result_of<F(Args)...>::type> submit(F &&f, Args &&...args) {

        using result_type = typename std::result_of<F(Args)...>::type;
        std::packaged_task<result_type()> task_(std::bind(std::forward<F>(f),
                                                          std::forward<Args>(args)...));
        std::future<result_type> res(task_.get_future());

        Task task(std::move(task_));
        std::future<result_type> dummy;

        if (_M_cores.size() < _M_coreSize) {
            addThread(defaultStrategy, true);
            _M_tasks.push(&task);
        } else if (_M_tasks.size() + 1 < _M_tasks.capacity()) {
            _M_tasks.push(&task);
        } else if (_M_tasks.size() + 1 == _M_tasks.capacity() && _M_cores.size() + _M_threads.size() < _M_poolSize) {
            addThread(defaultStrategy, false);
            _M_tasks.push(&task);
        } else {
            return dummy;
        }

        return res;
    }
};

#endif
