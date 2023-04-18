/*
 * @date   2023-04-03
 * @author anonymity
 *
 * @dev 实现一个最简单的线程池，由多个线程争抢提交的任务
 */

#ifndef IDLE_THREADPOOL_H
#define IDLE_THREADPOOL_H

#include <functional>
#include <mutex>
#include <vector>
#include <thread>
#include <future>
#include <atomic>
#include "SafeQueue.h"

class ThreadPool {
    class ThreadWorker {
        unsigned int m_id;
        ThreadPool *m_pool; // belong to
    public:
        ThreadWorker(ThreadPool *pool, const unsigned int id) : m_pool(pool), m_id(id) {}

        void operator()() const {
            std::function<void()> targetFunction;
            bool getElemFromQueue;

            while (!m_pool->m_shutdown) {
                {
                    std::unique_lock<std::mutex> lock(m_pool->m_conditional_mutex);

                    if (0 == m_pool->n_tasks) {
                        m_pool->m_conditional_lock.wait(lock);
                    }

                    getElemFromQueue = m_pool->m_queue.pop(targetFunction);
                    m_pool->n_tasks--;
                }

                if (getElemFromQueue) {
                    targetFunction();
                }
            }
        }
    };

    bool m_shutdown;
    std::atomic_uint32_t n_tasks;
    SafeQueue<std::function<void()>> m_queue;
    std::vector<std::thread> m_threads;
    std::mutex m_conditional_mutex;
    std::condition_variable m_conditional_lock;
public:
    ThreadPool(const int n_threads = 4) : m_threads(std::vector<std::thread>(n_threads)),
                                          m_shutdown(false),
                                          n_tasks(0) {}

    ThreadPool(const ThreadPool &) = delete;

    ThreadPool(ThreadPool &&) = delete;

    ThreadPool &operator=(const ThreadPool &) = delete;

    ThreadPool &operator=(ThreadPool &&) = delete;

    void init() {
        for (int i = 0; i < m_threads.size(); ++i) {
            m_threads.at(i) = std::thread(ThreadWorker(this, i)); // 分配工作线程
        }
    }

    // Waits until threads finish their current task and shutdowns the pool
    void shutdown() {
        /*
         * 如果当前的任务数量不为 0，则说明还需要继续执行，直至所有任务完成；
         * 在循环体中，我们使当前线程让出一段时间执行权，等待工作线程执行任务；
         * 此外，每次循环体中都唤醒一个线程来执行任务，确保不会出现无效等待
         */
        while (true) {
            {
                std::unique_lock<std::mutex> lock(m_conditional_mutex);
                if (0 == n_tasks) {
                    break;
                }
            }
            std::this_thread::yield();
            m_conditional_lock.notify_one();
        }
        m_shutdown = true;
        m_conditional_lock.notify_all(); // 通知，唤醒所有工作线程

        for (auto & m_thread : m_threads) {
            if (m_thread.joinable()) { // 判断线程是否在等待
                m_thread.join(); // 将线程加入到等待队列
            }
        }
    }

    // Submit a function to be executed asynchronously by the pool
    template<typename F, typename... Args>
    auto submit(F &&f, Args &&...args) -> std::future<decltype(f(args...))> {
        // Create a function with bounded parameter ready to execute
        std::function<decltype(f(args...))()> func = std::bind(std::forward<F>(f),
                                                               std::forward<Args>(args)...); // 连接函数和参数定义，特殊函数类型，避免左右值错误

        // Encapsulate it into a shared pointer in order to be able to copy construct
        auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);

        // Warp packaged task into void function
        std::function<void()> warpper_func = [task_ptr]() {
            (*task_ptr)();
        };

        // 队列通用安全封包函数，并压入安全队列
        m_queue.push(warpper_func);
        n_tasks++;

        // 唤醒一个等待中的线程
        m_conditional_lock.notify_one();

        // 返回先前注册的任务指针
        return task_ptr->get_future();
    }
};

#endif //IDLE_THREADPOOL_H
