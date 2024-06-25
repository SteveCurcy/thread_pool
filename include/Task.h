#ifndef TASK_H
#define TASK_H

#include <memory>
#include <future>
#include <utility>

/**
 * Task 类提供一个对任务的包装，允许存在空任务。
 * - 默认构造函数应该生成一个空任务，空任务的调用不会做任何事；
 * - 任务应该支持赋值和拷贝操作，但是一个任务只能由一个实例持有。
 *   如 Task a = b，则变量 b 失去对任务的所有权，而 a 获得。
 * 任务在最终将自动销毁，在析构函数中应该注意任务的释放。
 */
class Task {
    /* 提供一个接口，可以调用执行，可以通过多态完成析构 */
    struct task_base {
        virtual void call() = 0;
        virtual ~task_base() = default;
    };

    template<typename PT>
    struct task_impl : task_base {
        PT pt;  // 一个 packaged_task，可以用于执行等

        /* packaged_task 只能通过右值引用来传递 */
        explicit task_impl(PT &&_pt) : pt(std::move(_pt)) {}
        ~task_impl() = default;

        /* 任务的实现不允许拷贝和移动，只能通过指针转移所有权 */
        task_impl(const task_impl<PT> &other) noexcept = delete;
        task_impl(task_impl<PT> &&other) noexcept = delete;

        void call() override {
            /* 确保当前的任务是合法的，不合法的则不予执行；
             * 默认构造函数得到的任务是非法的 */
            if (pt.valid()) pt();
        }
    };
    
    std::unique_ptr<task_base> impl;
public:
    /**
     * 接收一个 std::packaged_task 类型的参数，将其包装依靠多态执行
     * @tparam PT 具体任务类型，std::packaged_task<T>，只能移动拷贝
     * @param _pt 实际的任务，只能移动传参
     */
    template<typename PT>
    explicit Task(PT &&_pt) : impl(new task_impl<PT>(std::forward<PT>(_pt))) {}

    Task(): impl(nullptr) {};

    ~Task() = default;

    Task(const Task &other) {
        Task& otherTask = const_cast<Task&>(other);
        impl.reset();
        impl.swap(otherTask.impl);
    }
    Task(Task &&other): impl(std::move(other.impl)) {
        other.impl.reset();
    }

    Task& operator=(const Task& other) noexcept {
        Task& otherTask = const_cast<Task&>(other);
        impl.reset();
        impl.swap(otherTask.impl);
        return *this;
    }
    Task& operator=(Task&& other) {
        impl.reset();
        impl.swap(other.impl);
        return *this;
    }

    void operator()() {
        if (impl) impl->call();
    }
};

#endif