/*
 * @author Xu.Cao
 * @date   2023-04-21
 * @detail 实现一个线程基类，用于提供线程的抽象
 *
 * @history
 *      <author>    <time>      <version>           <description>
 *      Xu.Cao      2023-04-20  0.1.2               创建此文件
 */

#ifndef THREAD_POOL_THREAD_BASE_H
#define THREAD_POOL_THREAD_BASE_H

#include <thread>

class thread_base {
protected:
    std::thread m_thread;
    bool f_is_shutdown = false;
    bool f_is_running = false;

    virtual void run() = 0;

public:
    thread_base() = default;

    ~thread_base() {
        if (!f_is_shutdown) {
            shutdown();
        }
    }

    void start() {
        m_thread = std::thread(&thread_base::run, this);
    }

    void shutdown() {
        f_is_shutdown = true;
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    bool get_running_flag() const {
        return f_is_running;
    }
};

#endif //THREAD_POOL_THREAD_BASE_H
