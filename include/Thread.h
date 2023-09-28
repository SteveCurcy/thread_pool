/**
 * @file Thread.h
 * @author Xu.Cao
 * @details
 *  本代码主要用于线程管理，由于线程管理部分耦合度较高，因此统一放置在本文件中。
 * 本模块主要包括：Thread、ThreadManager 三个大类，具体细节
 * 请参见类注释。
 */
#ifndef THREAD_H
#define THREAD_H

#include <thread>
#include <atomic>
#include <memory>
#include <functional>
#include <deque>

/* 此处是各个类的声明，主要为了后续交叉引用做准备 */
class Thread;
class ThreadManager;

/* 此处定义一些相关的类型，便于后续使用，将使用 using 而非 typedef */
using ThreadStatus = int;
using PoolStatus = unsigned int;   // status << 28 | count
/* Strategy 用于提供一个线程获取和执行任务的接口，策略应该作为线程和任务的粘合剂；
 * 进而实现线程管理和任务管理的解耦合，根据当前的任务数量、线程状态和线程池状态判断如何决策 */
using Strategy = std::function<size_t(const Thread&, const ThreadManager&)>;

/* 线程正在处于的状态 */
constexpr ThreadStatus STAT_SHUTDOWN = 0;   /* 线程执行以获取的剩余的任务，即将退出 */
constexpr ThreadStatus STAT_RUN      = 1;   /* 线程只在执行 */
/* 线程池正在处于的状态 */
constexpr int PoolStatusSize   = 32;
constexpr int PoolStatusOffset = 28;
constexpr PoolStatus PoolStatusMask = ((1 << (PoolStatusSize - PoolStatusOffset)) - 1) << PoolStatusOffset;
constexpr PoolStatus PoolCountMask = (1 << PoolStatusOffset) - 1;

/* 策略的函数声明 */
size_t defaultStrategy(const Thread& thrd, const ThreadManager& thrdMngr);

class Thread final {
    ThreadManager                *_M_affiliation;
    std::atomic<ThreadStatus>    _M_status;
    Strategy                     execStrategy;
    bool                         _M_isCore;
    std::unique_ptr<std::thread> _M_thread;
public:
    void run();

    /**
     * @param affiliation 从属的线程管理者
     * @param parent 生产当前线程的工厂
     * @param exec 线程的执行策略
     * @param core 是否为核心线程
     */
    Thread(ThreadManager *affiliation,
           const Strategy& exec, bool core):
            _M_affiliation(affiliation), _M_status(STAT_RUN),
            execStrategy(exec), _M_isCore(core) {
        _M_thread = std::unique_ptr<std::thread>(new std::thread(&Thread::run, this));
    }

    ~Thread() {
        if (_M_status.load(std::memory_order::memory_order_consume))
            shutdown(); // 如果没有关闭，才进行关闭
    }

    Thread(const Thread& other);
    Thread(Thread&& other) noexcept;
    Thread& operator=(const Thread& other);
    Thread& operator=(Thread&& other) noexcept;

    void shutdown() {
        _M_status.store(STAT_SHUTDOWN, std::memory_order::memory_order_release);
        _M_thread->join();
    }

    ThreadStatus getStatus() const { return _M_status.load(std::memory_order::memory_order_consume); }
    bool isCore() const { return _M_isCore; }
    void setStrategy(const Strategy& s) { execStrategy = s; }
};

class ThreadManager {
    size_t _M_coreSize;
    size_t _M_poolSize;
    std::atomic<PoolStatus> _M_status; // 
    /* 暂时只支持一个任务添加入口，因此对线程的操作也只有一个入口，
     * 所以此处不考虑多线程安全问题，使用 deque 存储已有线程 */
    std::deque<Thread> _M_threads;
public:
    ThreadManager(size_t core = 3, size_t pool = 10): _M_coreSize(core), _M_poolSize(pool) {}

    size_t size() const { return PoolCountMask & _M_status.load(std::memory_order::memory_order_consume); }

    Thread* addThread(const Strategy& strategy, bool isCore);
};

size_t dummy(const Thread& t, const ThreadManager& tm) { return 1; }

#endif
