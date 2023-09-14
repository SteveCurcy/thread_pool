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

    /**
     * 这里将原来的 unique_ptr 改为了 shared_ptr，在使用的时候需要注意不要循环引用；
     * 如果使用 unique_ptr 将导致对于 task 结构体的复制、赋值等操作必须使用移动语义；
     * 这不仅降低了代码的灵活性，而且容易产生代码错误，且 bug 不易排查，因此这里改用了
     * 安全性相对较低的 shared_ptr 以提升代码的灵活性和可用性；
     * 
     * 此外，虽然移动语义能在某种情况下能提升代码性能，但是要求在**大规模**无意义的
     * 拷贝情况下最好使用移动语义，但是对于简单类型的拷贝，建议不要滥用 move，实测性能无差别
     */
    std::shared_ptr<impl_base> impl;
public:
    /**
     * 接收一个 std::packaged_task 类型的参数，将其包装依靠多态执行
     * @tparam F 具体任务类型，std::packaged_task<T>，只能移动拷贝
     * @param f 实际的任务，只能移动传参
     */
    template<typename F>
    explicit Task(F &&f) : impl(new impl_type<F>(std::forward<F>(f))) {}

    Task(): impl(nullptr) {};

    Task(const Task &other): impl(other.impl) {}
    Task(Task &&other): impl(std::move(other.impl)) {}

    Task& operator=(const Task& other) noexcept = default;
    Task& operator=(Task&& other) noexcept = default;

    void operator()() {
        if (impl.use_count()) {
            impl->call();
        }
    }
};

#endif