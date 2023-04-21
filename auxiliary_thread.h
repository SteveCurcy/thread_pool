/*
 * @author Xu.Cao
 * @date   2023-04-20
 * @detail 线程池的辅助线程，当任务量超过核心线程负载时，
 *         线程池将开启辅助线程；辅助直接访问线程池的任务队列
 *
 * @history
 *      <author>    <time>      <version>           <description>
 *      Xu.Cao      2023-04-20  0.1.0               创建本文件
 *      Xu.Cao      2023-04-21  0.1.1               继承自基线程类
 */

#ifndef THREAD_POOL_AUXILIARY_THREAD_H
#define THREAD_POOL_AUXILIARY_THREAD_H

#include <thread>
#include "task.h"
#include "safe_queue.h"

class auxiliary_thread: public thread_base {
    safe_queue<task> *m_tasks;

    void run() override {
        std::vector<task> tasks;
        while (!f_is_shutdown) {
            if (m_tasks->pop_front(tasks, config::max_running_tasks_size) > 0) {
                for (task &task_: tasks) {
                    f_is_running = true;
                    task_();
                    f_is_running = false;
                }
                tasks.clear();
            } else {
                std::this_thread::yield();
            }
        }
    }
public:
    explicit auxiliary_thread(safe_queue<task> *tasks_in_pool): m_tasks(tasks_in_pool) {
        start();
    }

    ~auxiliary_thread() {
        if (!f_is_shutdown) {
            shutdown();
        }
    }
};

#endif //THREAD_POOL_AUXILIARY_THREAD_H
