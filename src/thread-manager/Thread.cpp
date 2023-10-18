#include "Thread.h"

void Thread::run() {
    int idleTime = 0;
    while (getStatus() != STAT_SHUTDOWN && idleTime < getIdleTime()) {
        if (!execStrategy(*this, *_M_affiliation)) {
            idleTime++;
            std::this_thread::yield();
		} else {
            idleTime = 0;
        }
    }
}

Thread::~Thread() {
    if (_M_status.load(std::memory_order::memory_order_consume))
        shutdown(); // 如果没有关闭，才进行关闭
    /* 当线程指针不为空时才将其从线程池中移除 */
    if (_M_thread)
        _M_affiliation->removeThread(_M_thread->get_id());
}

// Thread::Thread(const Thread& other) {
//     Thread& otherThread = const_cast<Thread&>(other);
//     _M_affiliation = otherThread._M_affiliation;
//     _M_status.store(otherThread._M_status.load(std::memory_order::memory_order_consume), std::memory_order::memory_order_release);
//     execStrategy = otherThread.execStrategy;
//     _M_thread = std::move(otherThread._M_thread);
// }

// Thread::Thread(Thread&& otherThread) noexcept {
//     _M_affiliation = otherThread._M_affiliation;
//     _M_status.store(otherThread._M_status.load(std::memory_order::memory_order_consume), std::memory_order::memory_order_release);
//     execStrategy = otherThread.execStrategy;
//     _M_thread = std::move(otherThread._M_thread);
// }

// Thread& Thread::operator=(const Thread& other) {
//     Thread& otherThread = const_cast<Thread&>(other);
//     _M_affiliation = otherThread._M_affiliation;
//     _M_status.store(otherThread._M_status.load(std::memory_order::memory_order_consume), std::memory_order::memory_order_release);
//     execStrategy = otherThread.execStrategy;
//     _M_thread = std::move(otherThread._M_thread);
//     return *this;
// }

// Thread& Thread::operator=(Thread&& otherThread) noexcept {
//     _M_affiliation = otherThread._M_affiliation;
//     _M_status.store(otherThread._M_status.load(std::memory_order::memory_order_consume), std::memory_order::memory_order_release);
//     execStrategy = otherThread.execStrategy;
//     _M_thread = std::move(otherThread._M_thread);
//     return *this;
// }

bool ThreadManager::addThread(const Strategy& strategy, bool isCore) {
    if (isCore && size() >= _M_coreSize) return false;
    if (!isCore && size() >= _M_poolSize) return false;
    
    Thread *t = new Thread(this, defaultStrategy, isCore);
    auto id = t->getId();
    if (isCore) _M_cores.emplace(id, t);
    else _M_threads.emplace(id, t);
    _M_status++;
    return true;
}

void ThreadManager::removeThread(std::thread::id id) {
    if (!_M_cores.count(id) && !_M_threads.count(id)) return;
    if (_M_cores.count(id)) {
        _M_cores.erase(id);
    } else {
        _M_threads.erase(id);
    }
}

ThreadManager::~ThreadManager() {
    while (!_M_tasks.empty()) {
        std::this_thread::yield();
    }
    /* 手动关闭所有线程，防止程序还在执行导致的程序错误；
     * 因为，线程的销毁和队列的销毁是无法确定的，所以应该
     * 手动关闭来确保线程不会访问已经销毁的队列 */
    for (auto& t: _M_threads) t.second->shutdown();
    for (auto& t: _M_cores) t.second->shutdown();
    printf("ThreadManager destruction: %lu tasks left.\n", _M_tasks.size());
}

size_t defaultStrategy(const Thread& thrd, ThreadManager& thrdMngr) {
    if (!thrd.isCore() && thrdMngr.getTaskQue().size() <= thrdMngr.getCore()) return 0;
    Task task;
    size_t cnt = thrdMngr.getTaskQue().pop(&task);
    if (cnt) {
        task();
    }
    return cnt;
}