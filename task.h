/*
 * @author Xu.Cao
 * @date   2023-04-14
 * @detail 实现对任务函数的封装，便于使用。
 *
 * @history
 *      <author>    <time>      <version>           <description>
 *      Xu.Cao      2023-04-14  0.0.2               创建了本代码
 *      Xu.Cao      2023-04-14  0.0.3               增加了函数指针判断，防止出现指针为空的函数调用
 */

#ifndef THREAD_TASK_H
#define THREAD_TASK_H

#include <memory>
#include <future>
#include <utility>

/**
 * 这个类将用来封装函数，防止出现 bad_function_call 错误。
 * 目标函数将是 std::packaged_task，因此只能移动赋值
 */
class task {
    struct impl_base {
        virtual void call() = 0;

        virtual ~impl_base() = default;
    };

    template<typename F>
    struct impl_type : impl_base {
        F f;

        explicit impl_type(F &&f_) : f(std::move(f_)) {}

        impl_type(impl_type<F> &&other) noexcept {
            f = std::move(other.f);
        }

        void call() override {
            f();
        }
    };

    std::unique_ptr<impl_base> impl;

public:
    /**
     * 接收一个 std::packaged_task 类型的参数，将其包装依靠多态执行
     * @tparam F 具体任务类型，std::packaged_task<T>
     * @param f 实际的任务，只能移动传参
     */
    template<typename F>
    explicit task(F &&f) : impl(new impl_type<F>(std::forward<F>(f))) {}

    task() = default;

    task(const task &) = delete;

    task(task &) = delete;

    task(task &&other) noexcept: impl(std::move(other.impl)) {}

    task &operator=(task &&other) noexcept {
        impl = std::move(other.impl);
        return *this;
    }

    task &operator=(const task &) = delete;

    /**
     * 任务的执行，确保任务不为空再执行
     */
    void operator()() {
        if (impl != nullptr) {
            impl->call();
        }
    }
};

#endif //THREAD_TASK_H
