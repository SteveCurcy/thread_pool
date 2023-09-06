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
#include <chrono>
#include <atomic>

class thread_base {
public:
    static constexpr int STAT_SHUT = 0;
    static constexpr int STAT_IDLE = 1;
    static constexpr int STAT_RUN = 2;
protected:
    std::thread m_thread;
    bool f_is_shutdown = false;
    bool f_is_running = false;

    std::atomic_int _F_status{STAT_RUN};
    std::atomic_ulong _M_avgWorkTime{1};   // unit us

    virtual void run() = 0;

public:
    thread_base() = default;

    ~thread_base() {
        if (_F_status.load(std::memory_order::memory_order_acquire)) {
            shutdown();
        }
    }

    void start() {
        m_thread = std::thread(&thread_base::run, this);
    }

    void shutdown() {
        _F_status.store(STAT_SHUT, std::memory_order::memory_order_consume);
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    unsigned long avgTaskCost() const {
        return _M_avgWorkTime;
    }

    bool get_running_flag() const {
        return f_is_running;
    }

    int getStatus() const {
        return _F_status.load(std::memory_order::memory_order_acquire);
    }
};

#endif //THREAD_POOL_THREAD_BASE_H
