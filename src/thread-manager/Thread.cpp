#include "Thread.h"

/**
 * 线程执行的模板，主要步骤为：
 * 从当前的队列中获取一个任务并且执行；
 * 在执行前后记录执行的时间，并计算实时平均任务执行时间
 */
void Thread::run() {
    while (_M_status.load(std::memory_order::memory_order_consume)) {
        Task task;
        if (_M_queue->pop(task)) {  // get a task from queue
            auto start = std::chrono::steady_clock::now();
            task();                 // execute the task
            size_t thisTaskTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
            _M_avgWorkTime = _M_realTimeFactor * (thisTaskTime - _M_avgWorkTime) + _M_avgWorkTime;
            _M_totWorkTime += thisTaskTime;
        } else {
            std::this_thread::yield();
        }
    }
}