#ifndef THREAD_H
#define THREAD_H

#include <thread>
#include <atomic>
#include <chrono>
#include <memory>
#include <functional>
#include "Task.h"
#include "LockFreeQueue.h"

typedef int ThreadStatus;
typedef size_t ThreadDuration;

constexpr ThreadStatus STAT_SHUT = 0;
constexpr ThreadStatus STAT_RUN  = 1;

class Thread {
    std::shared_ptr<DynamicLockFreeQueue<Task>> _M_queue;           // 用来存放任务的队列的指针，要执行任务时，从中取出
    std::atomic<ThreadStatus>                   _M_status;          // 线程的执行状态，当前只有执行和关闭两种状态，后续可丰富
    size_t                                      _M_batchSize;
    std::thread                                 _M_thread;

    size_t mono();
	size_t batch();
    size_t (Thread::*_M_work)();
    /* 执行策略，返回值代表是否执行了任务 */
public:
    virtual void run();

    Thread():
            _M_queue(nullptr), _M_status(STAT_RUN),
            _M_batchSize(3), _M_work(&Thread::batch) {}
    Thread(std::shared_ptr<DynamicLockFreeQueue<Task>> ptr):
            _M_queue(ptr), _M_status(STAT_RUN),
            _M_batchSize(3), _M_work(&Thread::batch) {
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
