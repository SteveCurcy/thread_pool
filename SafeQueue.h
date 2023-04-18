/*
 * @date   2023-04-03
 * @author anonymity
 *
 * @dev 实现一个多线程安全的队列，使用 mutex 临界资源进行多线程互斥的安全访问。
 */

#ifndef IDLE_SAFEQUEUE_H
#define IDLE_SAFEQUEUE_H


#include <queue>
#include <mutex>

template<typename T>
class SafeQueue {
    std::queue<T> m_queue;
    std::mutex m_mutex;

public:

    SafeQueue() = default;

    ~SafeQueue() = default;

    SafeQueue(SafeQueue &&other) noexcept {}

    void push(const T& t) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_queue.emplace(t);
    }

    bool pop(T &t) {
        std::unique_lock<std::mutex> lock(m_mutex);

        if (m_queue.empty()) {
            t = nullptr;
            return false;
        }

        t = std::move(m_queue.front());
        m_queue.pop();

        return true;
    }
};


#endif //IDLE_SAFEQUEUE_H
