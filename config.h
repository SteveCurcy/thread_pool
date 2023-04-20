/*
 * @author Xu.Cao
 * @data   2023-04-14
 * @detail this is a config file to configure the thread pool
 *
 * @history
 *      <author>    <time>      <version>           <description>
 *      Xu.Cao      2023-04-14  0.0.2               创建了本代码
 */

#ifndef THREAD_CONFIG_H
#define THREAD_CONFIG_H

using size_t = unsigned long;

class config {
public:
    static constexpr size_t core_threads_size = 2;
    static constexpr size_t max_running_tasks_group = 15;
    static constexpr size_t max_tasks_capacity = (max_running_tasks_group << 1);
};

#endif //THREAD_CONFIG_H
