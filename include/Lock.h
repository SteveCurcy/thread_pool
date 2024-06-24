#ifndef LOCK_H
#define LOCK_H

#include <atomic>
#include <memory>
#include <thread>

class spinLock
{
    std::atomic_flag flag{false};

public:
    void lock()
    {
        // 如果原有的值就是 true，则说明已经被加锁了，应该等待再尝试
        while (flag.test_and_set(std::memory_order_acq_rel))
        {
            std::this_thread::yield();
        }
    }

    void unlock()
    {
        flag.clear();
    }

    bool tryLock()
    {
        return !flag.test_and_set(std::memory_order_acq_rel);
    }
};

#endif