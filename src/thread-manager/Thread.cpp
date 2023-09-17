#include "Thread.h"
#include <iostream>

bool Thread::mono() {
    Task task;
    int ret = _M_queue->pop(&task);
    if (ret) {
        task();
    }
    return ret;
}

bool Thread::batch() {
    std::vector<Task> tasks(_M_batchSize);
    int ret = _M_queue->pop(&tasks[0], _M_batchSize);
    if (ret) {
        for (int i = 0; i < ret; i++) {
            tasks[i]();
        }
    }
    return ret;
}

/**
 * 线程执行的模板，主要步骤为：
 * 从当前的队列中获取一个任务并且执行；
 * 在执行前后记录执行的时间，并计算实时平均任务执行时间
 */
void Thread::run() {
    while (_M_status.load(std::memory_order::memory_order_consume)) {
        auto start = std::chrono::steady_clock::now();
        if ((this->*_M_work)()) {
            size_t thisTaskTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
            _M_avgWorkTime = _M_realTimeFactor * (thisTaskTime - _M_avgWorkTime) + _M_avgWorkTime;
            _M_totWorkTime += thisTaskTime;
        } else {
            std::this_thread::yield();
        }
    }
}