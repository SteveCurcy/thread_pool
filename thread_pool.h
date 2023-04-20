/*
 * @author Xu.Cao
 * @date   2023-04-14
 * @detail 实现了线程池，可以在获取任务后决定线程通过何种方式执行，并将任务分派给核心线程
 *
 * @history
 *      <author>    <time>      <version>           <description>
 *      Xu.Cao      2023-04-14  0.0.2               创建了本代码
 *      Xu.Cao      2023-04-16  0.0.3               1. 修改了不安全的任务队列的使用
 *                                                  2. 增加了对任务数量的维护，从而更好地对线程池进行控制
 *      Xu.Cao      2023-04-20  0.0.4               1. 删除批量模式控制，所有线程均批量执行，通过任务窃取实现任务的均摊
 *                                                  2. 增加了对提交任务的限制，关闭线程池不能提交任务，增强健壮性
 */

#ifndef IDLE_THREAD_POOL_H
#define IDLE_THREAD_POOL_H

#include <mutex>
#include <vector>
#include <thread>
#include <future>
#include <functional>
#include "core_thread.h"

class thread_pool {
    safe_queue<task> m_tasks;
    std::vector<core_thread *> m_core_threads;
    std::thread m_manager;
    bool f_is_shutdown = false;

    /**
     * 将任务队列中的任务分配到核心线程中，为了尽可能的平均分配，
     * 尝试每个核心线程分配 1/3 的任务量，如果太多则分配配置文件
     * 设定的最大任务数
     */
    void dispatch() {
        size_t size_for_each = config::max_running_tasks_group;

        for (auto *core: m_core_threads) {
            core->fill_cache(m_tasks, size_for_each);
        }
    }

    /**
     * 管理线程，用于分配任务和指定核心线程的执行模式，批量执行还是单个执行；
     * 这里我们发现只要超过单个线程的最大任务数量就修改为批量执行，否则为单个执行；
     * 如果线程池关闭还有任务没有完成，则继续分配任务，直到任务数量不再过多，
     * 则剩余的任务交由管理线程自己执行
     */
    void manage() {
        while (!f_is_shutdown) {
            dispatch();
        }

        /*
         * 到此线程池已经被结束，但是任务队列还有剩余的任务，所以还要按需求继续分配；
         * 如果任务的数量仍然不低于最大任务量，则继续分配，然后剩下的任务将在本线程完成；
         * @NOTE 此外，这里使用了不安全的 size 函数，因为只用做估计，可以牺牲忽略不计的
         *       安全性问题，提升程序性能
         */
        while (m_tasks.size() >= config::max_tasks_capacity) {
            dispatch();
        }

        /* 如果还有剩余的任务，则直接自己处理 */
        std::vector<task> tasks_;
        while (m_tasks.pop_front(tasks_) != -1) {
            std::this_thread::yield();
        }
        for (task &task_: tasks_) {
            task_();
        }
        tasks_.clear();
    }

public:
    explicit thread_pool() : m_core_threads(std::vector<core_thread *>(config::core_threads_size)) {}

    ~thread_pool() {
        if (!f_is_shutdown) {
            shutdown();
        }
    }

    thread_pool(const thread_pool &) = delete;

    thread_pool(thread_pool &&) = delete;

    thread_pool &operator=(const thread_pool &) = delete;

    thread_pool &operator=(thread_pool &&) = delete;

    /**
     * 开启线程池，首先根据配置文件，创建指定数目的核心线程，并开启
     * 开启管理线程，开始线程池的调度
     */
    void start() {
        f_is_shutdown = false;

        for (auto *&core: m_core_threads) {
            core = new core_thread(m_core_threads);
        }

        for (auto *core: m_core_threads) {
            core->start();
        }

        m_manager = std::thread(&thread_pool::manage, this);
    }

    /**
     * 首先确保管理线程完成任务分配和残留任务；
     * 其次，确保每个核心线程完成各自的任务并回收资源
     */
    void shutdown() {
        f_is_shutdown = true;

        m_manager.join();

        for (auto *core: m_core_threads) {
            core->shutdown();
        }

        for (auto *core: m_core_threads) {
            delete core;
        }
    }

    /**
     * 任务的提交，将任意函数和对应参数封装成一个任务保存到任务队列中
     * @tparam F 函数类型
     * @tparam Args 可变参数类型
     * @param f 任务函数
     * @param args 任务所需的参数
     * @return future，可以获取当前任务的执行情况和获取返回值
     */
    template<typename F, typename... Args>
    std::future<typename std::result_of<F()>::type> submit(F &&f, Args &&...args) {

        if (f_is_shutdown) {
            throw std::runtime_error("线程池已经被关闭，正在执行残余任务，不能继续提交任务！");
        }

        using result_type = typename std::result_of<F()>::type;
        std::packaged_task<result_type()> task_(std::bind(std::forward<F>(f),
                                                          std::forward<Args>(args)...));
        std::future<result_type> res(task_.get_future());

        m_tasks.push(task(std::move(task_)));

        return res;
    }
};

#endif //IDLE_THREAD_POOL_H
