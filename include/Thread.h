#ifndef THREAD_H
#define THREAD_H

#include <thread>
#include <atomic>
#include <chrono>
#include <memory>
#include "Task.h"
#include "LockFreeQueue.h"

typedef int ThreadStatus;
typedef size_t ThreadDuration;

constexpr ThreadStatus STAT_SHUT = 0;
constexpr ThreadStatus STAT_RUN  = 1;

class Thread {
    std::thread                                 _M_thread;
    std::atomic<ThreadStatus>                   _M_status;          // 线程的执行状态，当前只有执行和关闭两种状态，后续可丰富
    std::shared_ptr<DynamicLockFreeQueue<Task>> _M_queue;           // 用来存放任务的队列的指针，要执行任务时，从中取出
    std::chrono::steady_clock::time_point       _M_startTime;       // 线程开启时间
    ThreadDuration                              _M_totWorkTime;     // 工作总时间 (us)
    ThreadDuration                              _M_avgWorkTime;     // 平均任务执行时间 (us)
    float                                       _M_realTimeFactor;  // 实时性因子，用来影响平均执行时间的因子，因子越大，受最近任务影响越大
public:
    virtual void run();

    Thread():
            _M_queue(nullptr), _M_status(STAT_RUN),
            _M_startTime(std::chrono::steady_clock::now()),
            _M_totWorkTime(0), _M_avgWorkTime(1) {}
    Thread(std::shared_ptr<DynamicLockFreeQueue<Task>> ptr):
            _M_queue(ptr), _M_status(STAT_RUN),
            _M_startTime(std::chrono::steady_clock::now()),
            _M_totWorkTime(0), _M_avgWorkTime(1) {
        _M_thread = std::thread(&Thread::run, this);
    }
    virtual ~Thread() {
        if (_M_status.load(std::memory_order::memory_order_consume))
            shutdown(); // 如果没有关闭，才进行关闭
    }

    virtual void shutdown() {
        _M_status.store(STAT_SHUT, std::memory_order::memory_order_release);
        _M_thread.join();
    }

    void start(std::shared_ptr<DynamicLockFreeQueue<Task>> ptr) {
        _M_queue = ptr;
        _M_thread = std::thread(&Thread::run, this);
    }
};

#endif