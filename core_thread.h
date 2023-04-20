/*
 * @author Xu.Cao
 * @date   2023-04-14
 * @detail 实现一个核心线程，核心线程将包括一个本地任务队列缓存，
 *      此外，核心线程除了可以访问缓存，还可以访问其他核心线程的
 *      任务缓存，从而偷取其他核心的任务
 *
 * @history
 *      <author>    <time>      <version>           <description>
 *      Xu.Cao      2023-04-14  0.0.2               创建了本代码
 *      Xu.Cao      2023-04-16  0.0.3               修复了触发 bad_function_call 的 bug
 *      Xu.Cao      2023-04-20  0.0.4               1. 删除了批量执行模式的修改 f_is_batch，只能批量执行；
 *                                                  2. 由于只能批量执行，所以线程是否正在运行不重要，删除 f_is_running
 */

#ifndef THREAD_CORE_THREAD_H
#define THREAD_CORE_THREAD_H

#include <vector>
#include <thread>
#include <queue>
#include "safe_queue.h"
#include "config.h"
#include "task.h"
#include "config.h"

class core_thread {
    std::vector<core_thread *> &core_pool_threads;
    safe_queue<task> m_tasks_cache;
    std::thread m_thread;
    bool f_is_shutdown = false;
    bool f_is_running = false;

    bool get_tasks_from_cache(std::vector<task> &tasks) {
        return m_tasks_cache.pop_front(tasks, config::max_running_tasks_group) > 0;
    }

    bool steal_tasks_from_pool(std::vector<task> &tasks) {
        bool result = false;

        for (auto *core: core_pool_threads) {
            if (core == this) {
                continue;
            }
            if (core->m_tasks_cache.pop_back(tasks, config::max_running_tasks_group, (config::max_tasks_capacity / 5)) > 0) {
                result = true;
                break;
            }
        }

        return result;
    }

    void run() {
        if (std::any_of(core_pool_threads.begin(), core_pool_threads.end(),
                        [](core_thread *core) { return nullptr == core; })) {
            throw std::runtime_error("Init error: core thread pointer is null!");
        }

        std::vector<task> tasks;
        while (!f_is_shutdown) {
            if (get_tasks_from_cache(tasks) || steal_tasks_from_pool(tasks)) {
                for (auto &&task_: tasks) {
                    task_();
                }
                tasks.clear();
            } else {
                std::this_thread::yield();
            }
        }

        while (m_tasks_cache.pop_front(tasks) == -1) {
            std::this_thread::yield();
        }
        for (task &task_: tasks) {
            task_();
        }
        tasks.clear();
    }

public:
    explicit core_thread(std::vector<core_thread *> &cores) : core_pool_threads(cores) {}

    ~core_thread() {
        if (!f_is_shutdown) {
            shutdown();
        }
    }

    size_t fill_cache(safe_queue<task> &tasks, size_t batch_size) {
        return m_tasks_cache.push(tasks, batch_size, (batch_size << 1));
    }

    void start() {
        m_thread = std::thread(&core_thread::run, this);
    }

    void shutdown() {
        f_is_shutdown = true;
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }
};

#endif //THREAD_CORE_THREAD_H
