/*
 * @author Xu.Cao
 * @data   2023-04-14
 * @detail 这是线程池的一个配置文件
 *
 * @history
 *      <author>    <time>      <version>           <description>
 *      Xu.Cao      2023-04-14  0.0.2               创建了本代码
 *      Xu.Cao      2023-04-20  0.0.4               增加了线程执行任务数量和线程任务缓存容量的区分
 *      Xu.Cao      2023-04-22  0.1.3               增加 ttl 用来控制何时关闭辅助线程
 */

#ifndef THREAD_CONFIG_H
#define THREAD_CONFIG_H

#include <thread>
#include <algorithm>

using size_t = unsigned long;

class config final {
public:
    static constexpr size_t core_threads_size = 3;
    static constexpr size_t max_running_tasks_size = 15;
    static constexpr size_t max_tasks_capacity = (max_running_tasks_size << 1);
    static constexpr size_t time_to_live = 3;
    static const size_t max_auxiliary_threads_size;
};

const size_t config::max_auxiliary_threads_size = std::thread::hardware_concurrency() > 3 ? std::thread::hardware_concurrency() - config::core_threads_size: 3;

#endif //THREAD_CONFIG_H
