/*
 * @author Xu.Cao
 * @date   2023-04-14
 * @detail 实现一个多线程安全的队列，使用 mutex 临界资源进行多线程互斥的安全访问。
 *
 * @history
 *      <author>    <time>      <version>           <description>
 *      Xu.Cao      2023-04-14  0.0.2               创建了本代码
 *      Xu.Cao      2023-04-16  0.0.3               1. 修改了所有弹出和插入代码，细化返回状态，详情见注释
 *                                                  2. 删除了 empty, size, plunder 不安全的函数
 *                                                  3. 加入了一次性弹出函数和从另一个安全队列插入的函数
 */

#ifndef IDLE_SAFE_QUEUE_H
#define IDLE_SAFE_QUEUE_H


#include <deque>
#include <queue>
#include <vector>
#include "spin_lock.h"

template<typename T>
class safe_queue {
    std::deque<T> m_deque;
    spin_lock m_mutex;

public:

    safe_queue() = default;

    ~safe_queue() = default;

    safe_queue(safe_queue &&) = delete;

    /**
     * 将一个元素插入到队列的末尾
     * @param t 需要插入的元素
     */
    void push(T &&t) {
        while (true) {
            if (m_mutex.try_lock()) {
                m_deque.emplace_back(std::move(t));
                m_mutex.unlock();
                break;
            } else {
                std::this_thread::yield();
            }
        }
    }

    /**
     * 将一个队列中的一批数据插入到队列的末尾
     * @param ts 要插入的数据队列
     * @param batch_size 要插入的元素数量
     * @param max_saved 队列最多可以保存的元素数量
     * @return 返回实际插入的元素数量
     */
    size_t push(std::queue<T> &ts, size_t batch_size, size_t max_saved) {
        size_t push_size = 0;

        while (true) {
            if (m_mutex.try_lock()) {
                while (!ts.empty() && m_deque.size() < max_saved && batch_size--) {
                    m_deque.emplace_back(std::move(ts.front()));
                    ts.pop();
                    push_size++;
                }
                m_mutex.unlock();
                break;
            } else {
                std::this_thread::yield();
            }
        }

        return push_size;
    }

    /**
     * 将另一个 safe_queue 中的元素弹出并插入到当前队列的末尾
     * @param from_ 源安全队列
     * @param max_batch 最多弹出和插入的元素数量
     * @param max_saved 一个队列最多能接收的元素数量
     * @return 实际弹出和插入的元素数量
     */
    size_t push(safe_queue<T> &from_, size_t max_batch, size_t max_saved) {
        size_t push_cnt_ = 0;

        while (true) {
            if (from_.m_mutex.try_lock()) {
                m_mutex.lock();

                while (!from_.m_deque.empty() && m_deque.size() < max_saved && max_batch--) {
                    m_deque.emplace_back(std::move(from_.m_deque.front()));
                    from_.m_deque.pop_front();
                    push_cnt_++;
                }

                m_mutex.unlock();
                from_.m_mutex.unlock();
                break;
            } else {
                std::this_thread::yield();
            }
        }

        return push_cnt_;
    }

    /**
     * 尝试从首部弹出一个元素
     * @param t 保存弹出的元素
     * @return 如果返回 1，则弹出成功，0 则是队列是空
     *         如果返回 -1，则未抢到锁
     */
    int pop_front(T &t) {
        int pop_cnt = 0;

        if (m_mutex.try_lock()) {
            if (!m_deque.empty()) {
                t = std::move(m_deque.front());
                m_deque.pop_front();
                pop_cnt = 1;
            }
            m_mutex.unlock();
        } else {
            pop_cnt = -1;
        }

        return pop_cnt;
    }

    /**
     * 尝试从队列首部弹出一批元素，将元素存储到 ts 中
     * @param ts 保存弹出的一批元素
     * @param max_batch 弹出元素的数量
     * @return 如果返回非负数，则代表实际弹出元素的数量
     *         如果返回 -1，则未抢到锁
     */
    int pop_front(std::vector<T> &ts, size_t max_batch) {
        int pop_cnt = 0;

        if (m_mutex.try_lock()) {
            while (!m_deque.empty() && max_batch--) {
                ts.emplace_back(std::move(m_deque.front()));
                m_deque.pop_front();
                pop_cnt++;
            }
            m_mutex.unlock();
        } else {
            pop_cnt = -1;
        }

        return pop_cnt;
    }

    /**
     * 将队列中的所有元素都弹出
     * @param ts 接收所有弹出的元素
     * @return 如果返回非负数，则代表实际弹出元素的数量
     *         如果返回 -1，则未抢到锁
     */
    int pop_front(std::vector<T> &ts) {
        int pop_cnt = 0;

        if (m_mutex.try_lock()) {
            while (!m_deque.empty()) {
                ts.emplace_back(std::move(m_deque.front()));
                m_deque.pop_front();
                pop_cnt++;
            }
        } else {
            pop_cnt = -1;
        }

        return pop_cnt;
    }

    /**
     * 尝试从队列末尾弹出一个元素
     * @param t 保存弹出的元素
     * @return 如果返回 1，则弹出成功，0 则是队列是空
     *         如果返回 -1，则未抢到锁
     */
    int pop_back(T &t) {
        int pop_cnt = 0;

        if (m_mutex.try_lock()) {
            if (!m_deque.empty()) {
                t = std::move(m_deque.back());
                m_deque.pop_back();
                pop_cnt = 1;
            }
            m_mutex.unlock();
        } else {
            pop_cnt = -1;
        }

        return pop_cnt;
    }

    /**
     * 尝试从末尾弹出一批元素
     * @param ts 保存弹出的元素
     * @param max_batch 最多弹出的元素数量
     * @param min_saved 源队列中保留的最少元素数量
     * @return 如果返回非负数，则代表实际弹出元素的数量
     *         如果返回 -1，则未抢到锁
     */
    int pop_back(std::vector<T> &ts, size_t max_batch, size_t min_saved) {
        int result = 0;

        if (m_mutex.try_lock()) {
            if (m_deque.size() >= min_saved) {
                while (m_deque.size() > min_saved && max_batch--) {
                    ts.emplace_back(std::move(m_deque.back()));
                    m_deque.pop_back();
                    result++;
                }
            }
            m_mutex.unlock();
        } else {
            result = -1;
        }

        return result;
    }
};


#endif //IDLE_safe_queue_H
