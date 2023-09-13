#ifndef TASK_CACHE_H
#define TASK_CACHE_H

#include "Task.h"
#include "LockFreeQueue.h"
#include <functional>
#include <vector>
#include <memory>
#include <queue>

constexpr int DEFAULT_QUEUE_NR = 6; // 缓存中最多使用六个无锁队列
int MAX_QUEUE_SIZE = 2000;  // 每个无锁队列最大调整为 2k 大小，保留

/**
 * 本类作为任务和线程组中间的缓冲层，其中包含多个无锁任务队列。
 * 当一个任务到达时，任务缓存类会将其尽可能均匀地分配到不同队列，
 * 从而平衡不同的线程工作。
 * 
 * 一个工作线程将会指向一个任务缓存中的队列，其指向将由本类决定，
 * 从而尽可能减少线程之间对任务队列的争抢。
 * 
 * **NOTE** 不要尝试将线程的一些信息添加到本类，这会使得两个类
 * 耦合度过高，在后续的更新和问题排查中都会更困难。
 * 
 * 具体需求：
 * 1. getQueuePtr 获取一个队列的指针，用于给线程用于获取任务
 * 2. submit 用于提交一个任务，由当前类添加到合适的队列中
 */
class TaskCache final {
public:
    /* 定义一些常用的类型 */
    typedef DynamicLockFreeQueue<Task> DLFQueue;
    typedef std::shared_ptr<DLFQueue> QueuePtr;
    typedef TaskCache self;
private:
    std::vector<QueuePtr> _M_queues;
public:
    TaskCache(int queueNr = DEFAULT_QUEUE_NR): _M_queues(queueNr, nullptr) {
        for (auto& ptr: _M_queues) {
            ptr.reset(new DLFQueue);
        }
    }

    /* 找一个引用数量最少的队列指针 */
    const QueuePtr& getQueuePtr();

    /* 提交一个任务，找一个任务量最少的队列添加进去 */
    template<typename F, typename... Args>
    std::future<typename std::result_of<F(Args...)>::type> submitNonBlock(F&& f, Args&&... args) {
        using result_type = typename std::result_of<F(Args...)>::type;
        std::packaged_task<result_type()> task_(std::bind(std::forward<F>(f),
                                                            std::forward<Args>(args)...));
        std::future<result_type> ret(task_.get_future()), dummy;

        int propQueId = 0, cnt = 0;
        int minTask = 9999;
        for (auto& elem: _M_queues) {
            /* 这里引用数量为 1，是 TaskCache 成员变量本身的引用 */
            if (elem.use_count() <= 1) continue;
            auto taskNr = elem->size();
            if (taskNr < minTask) {
                propQueId = cnt;
                minTask = taskNr;
            }
        }
        if (!_M_queues[propQueId]->push(Task(std::move(task_)))) return dummy;
        return ret;
    }
};

#endif