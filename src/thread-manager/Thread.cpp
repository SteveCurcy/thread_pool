#include "Thread.h"
#include <iostream>

size_t Thread::mono() {
    Task task;
    auto ret = _M_queue->pop(&task);
    if (ret) {
        task();
    }
    return ret;
}

size_t Thread::batch() {
    std::vector<Task> tasks(_M_batchSize);
    auto ret = _M_queue->pop(&tasks[0], _M_batchSize);
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
 */
void Thread::run() {
    while (_M_status.load(std::memory_order::memory_order_consume)) {
        if (!((this->*_M_work)())) {
            std::this_thread::yield();
		}
    }
}
