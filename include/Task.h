#ifndef TASK_H
#define TASK_H

#include <memory>
#include <future>
#include <utility>

/**
 * 这个类将用来封装函数，防止出现 bad_function_call 错误。
 * 目标函数将是 std::packaged_task，因此只能移动赋值
 */
class Task {
    struct impl_base {
        virtual void call() = 0;
        virtual ~impl_base() = default;
    };

    template<typename F>
    struct impl_type : impl_base {
        F f;

        explicit impl_type(F &&f_) : f(std::move(f_)) {}

        impl_type(const impl_type<F> &other) noexcept = delete;
        impl_type(impl_type<F> &&other) noexcept = default;

        void call() override {
            f();
        }
    };
    
    std::unique_ptr<impl_base> impl;
public:
    /**
     * 接收一个 std::packaged_task 类型的参数，将其包装依靠多态执行
     * @tparam F 具体任务类型，std::packaged_task<T>，只能移动拷贝
     * @param f 实际的任务，只能移动传参
     */
    template<typename F>
    explicit Task(F &&f) : impl(new impl_type<F>(std::forward<F>(f))) {}

    Task(): impl(nullptr) {};

    Task(const Task &other) {
        Task& otherTask = const_cast<Task&>(other);
        impl = std::move(otherTask.impl);
    }
    Task(Task &&other): impl(std::move(other.impl)) {}

    Task& operator=(const Task& other) noexcept {
        Task& otherTask = const_cast<Task&>(other);
        impl = std::move(otherTask.impl);
        return *this;
    }
    Task& operator=(Task&& other) noexcept = default;

    void operator()() {
        if (impl) {
            impl->call();
        }
    }
};

#endif