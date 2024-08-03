/**
 * @file Lock.h
 * @brief 自旋锁实现头文件
 *
 * 本文件定义了spinLock类，该类提供了一种基于std::atomic_flag的「自旋锁」实现。
 * 自旋锁是一种低级的同步原语，用于多线程编程中的互斥访问，适用于占用「临界资源」时间非常
 * 短的场景。
 *
 * 当线程尝试获取锁时，如果锁已被其他线程占用，则当前线程会进入忙等待（自旋）状态，
 * 直到锁变为可用。这种机制在锁持有时间非常短的情况下非常有效，因为它避免了线程切换的开销。
 *
 * @author Xu.Cao
 * @date 2024/08/03
 * @copyright 2024 Xu.Cao, All rights reserved
 */
#ifndef LOCK_H
#define LOCK_H

#include <atomic>
#include <memory>
#include <thread>

/**
 * @class spinLock
 * @brief 自旋锁实现
 *
 * spinLock 类提供了基本的自旋锁功能，用于多线程同步，使用 std::atomic_flag 作为锁的标志，
 * 以确保线程安全。
 *
 * 提供了 lock() 方法用于加锁，unlock() 方法用于解锁，以及 tryLock() 方法尝试加锁但立即
 * 返回结果。
 */
class spinLock
{
    std::atomic_flag flag{false}; // 原子标志，用于表示锁的状态

public:
    /**
     * @brief 加锁
     *
     * 如果锁已被其他线程占用，则循环等待直到锁变为可用。
     * 使用 std::atomic_flag 的 test_and_set 方法来检查并设置锁的状态。
     * 如果锁已被占用（即标志为 true），则当前线程会调用 std::this_thread::yield() 来
     * 放弃 CPU 时间片，让其他线程有机会运行。
     */
    void lock()
    {
        while (flag.test_and_set(std::memory_order_acq_rel))
        {
            std::this_thread::yield();
        }
    }

    /**
     * @brief 解锁
     *
     * 将锁的状态设置为未锁定（即将 std::atomic_flag 标志设置为 false）。
     * 这允许其他等待锁的线程继续执行。
     */
    void unlock()
    {
        flag.clear();
    }

    /**
     * @brief 尝试加锁
     *
     * 尝试加锁，但立即返回结果，不会等待。
     * 如果锁未被占用（即标志为 false），则将其设置为 true 并返回 true，表示加锁成功。
     * 如果锁已被占用，则返回 false，表示加锁失败。
     *
     * @return bool 如果成功加锁则返回 true，否则返回 false。
     */
    bool tryLock()
    {
        return !flag.test_and_set(std::memory_order_acq_rel);
    }
};

#endif