/*
 * @author  Xu.Cao (Steve Curcy)
 * @date    2023-04-08
 * @details spin_lock implemented by atomic_flag.
 * @classes
 *      spin_lock belongs to class safe_queue, it uses atomic_flag to
 *          implement a self-spin lock, which will not cause thread-context
 *          exchange and give a better performance in short jobs.
 *
 * @history
 *      <author>    <time>      <version>           <description>
 *      Xu.Cao      2023-04-09  0.0.1               create a spin_lock
 */

#ifndef THREAD_SPIN_LOCK_H
#define THREAD_SPIN_LOCK_H

#include <atomic>

class spin_lock {
    std::atomic_flag m_lock;
public:
    spin_lock() : m_lock(false) {}

    void lock() { while (m_lock.test_and_set(std::memory_order_acquire)); }

    void unlock() { m_lock.clear(std::memory_order_release); }

    bool try_lock() {
        return !m_lock.test_and_set();
    }
};

#endif //THREAD_SPIN_LOCK_H
