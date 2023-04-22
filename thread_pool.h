/*
 * @author Xu.Cao
 * @date   2023-04-14
 * @detail 实现了线程池，可以在获取任务后决定线程通过何种方式执行，并将任务分派给核心线程
 *
 * @history
 *      <author>    <time>      <version>           <description>
 *      Xu.Cao      2023-04-14  0.0.2               创建了本代码
 *      Xu.Cao      2023-04-16  0.0.3               修改了不安全的任务队列的使用
 *      Xu.Cao      2023-04-20  0.0.4               1. 删除批量模式控制，所有线程均批量执行，通过任务窃取实现任务的均摊
 *                                                  2. 增加了对提交任务的限制，关闭线程池不能提交任务，增强健壮性
 *      Xu.Cao      2023-04-21  0.1.1               增加了辅助线程，当任务量过多时，逐个开启辅助线程，在任务量较少时关闭
 *      Xu.Cao      2023-04-22  0.1.3               加入了辅助线程存活时间，当发现辅助线程持续空闲则将他们关闭
 */

#ifndef IDLE_THREAD_POOL_H
#define IDLE_THREAD_POOL_H

#include <mutex>
#include <vector>
#include <thread>
#include <future>
#include <functional>
#include "core_thread.h"
#include "auxiliary_thread.h"

class thread_pool {
    safe_queue<task> m_tasks;
    std::vector<core_thread *> m_core_threads;
    std::vector<auxiliary_thread *> m_auxiliary_threads;
    std::thread m_manager;
    size_t c_aux_idle = 0;    // 超过 ttl 说明空闲，则回收辅助线程
    bool f_is_shutdown = false;

    /**
     * 将任务队列中的任务分配到核心线程中，为每个线程分配最大容量数量的任务；
     * 其实就是分配两倍的最大批量运行任务数量，结合任务窃取实现任务均摊
     */
    void dispatch() {
        size_t size_for_each = config::max_tasks_capacity;

        for (auto *core: m_core_threads) {
            core->fill_cache(m_tasks, size_for_each);
        }

        /* 分配完任务之后，查看辅助线程的运行状态 */
        bool is_idle_ = true;
        for (auto *aux: m_auxiliary_threads) {
            if (aux->get_running_flag()) {
                is_idle_ = false;
                break;
            }
        }
        if (is_idle_) {
            c_aux_idle++;
        }

        if (m_tasks.size() >= (config::max_tasks_capacity << 1)) {
            // 查看当前任务量，使用不安全的任务队列数量，进行估计
            // 如果当前任务分配完后还有大量的任务，则尝试开启辅助线程
            if (m_auxiliary_threads.size() < config::max_auxiliary_threads_size) {
                m_auxiliary_threads.emplace_back(new auxiliary_thread(&m_tasks));
            }
        } else if (c_aux_idle >= config::time_to_live) {
            // 如果发现所有辅助线程都空转，说明当前已经不需要辅助线程并将它们移除
            for (auto *aux: m_auxiliary_threads) {
                delete aux;
            }
            m_auxiliary_threads.clear();
        }
    }

    /**
     * 如果线程池关闭还有任务没有完成，则继续分配任务，直到任务数量不再过多，
     * 则剩余的任务交由管理线程自己执行
     */
    void manage() {
        while (!f_is_shutdown) {
            dispatch();
            std::this_thread::yield();
        }

        /*
         * 由于线程池被关闭，不再允许有任务被提交，线程池进入任务的收尾阶段，
         * 直接关闭辅助线程，将剩余的任务交由核心线程和管理线程来完成
         */
        for (auto *auxiliary: m_auxiliary_threads) {
            delete auxiliary;
        }
        m_auxiliary_threads.clear();

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
