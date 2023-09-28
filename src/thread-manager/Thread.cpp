#include "Thread.h"

void Thread::run() {
    while (_M_status.load(std::memory_order::memory_order_consume)) {
        if (!execStrategy(*this, *_M_affiliation)) {
            std::this_thread::yield();
		}
    }
}

Thread::Thread(const Thread& other) {
    Thread& otherThread = const_cast<Thread&>(other);
    _M_affiliation = otherThread._M_affiliation;
    _M_status.store(otherThread._M_status.load(std::memory_order::memory_order_consume), std::memory_order::memory_order_release);
    execStrategy = otherThread.execStrategy;
    _M_isCore = otherThread._M_isCore;
    _M_thread = std::move(otherThread._M_thread);
}

Thread::Thread(Thread&& otherThread) noexcept {
    _M_affiliation = otherThread._M_affiliation;
    _M_status.store(otherThread._M_status.load(std::memory_order::memory_order_consume), std::memory_order::memory_order_release);
    execStrategy = otherThread.execStrategy;
    _M_isCore = otherThread._M_isCore;
    _M_thread = std::move(otherThread._M_thread);
}

Thread& Thread::operator=(const Thread& other) {
    Thread& otherThread = const_cast<Thread&>(other);
    _M_affiliation = otherThread._M_affiliation;
    _M_status.store(otherThread._M_status.load(std::memory_order::memory_order_consume), std::memory_order::memory_order_release);
    execStrategy = otherThread.execStrategy;
    _M_isCore = otherThread._M_isCore;
    _M_thread = std::move(otherThread._M_thread);
    return *this;
}

Thread& Thread::operator=(Thread&& otherThread) noexcept {
    _M_affiliation = otherThread._M_affiliation;
    _M_status.store(otherThread._M_status.load(std::memory_order::memory_order_consume), std::memory_order::memory_order_release);
    execStrategy = otherThread.execStrategy;
    _M_isCore = otherThread._M_isCore;
    _M_thread = std::move(otherThread._M_thread);
    return *this;
}

Thread* ThreadManager::addThread(const Strategy& strategy, bool isCore) {
    if (isCore && size() >= _M_coreSize) return nullptr;
    if (!isCore && size() >= _M_poolSize) return nullptr;
    if (isCore) {
        _M_threads.emplace_front(this, dummy, isCore);
        _M_status++;
        return &_M_threads[0];
    } else {
        _M_threads.emplace_back(this, dummy, isCore);
        _M_status++;
        return &_M_threads[size() - 1];
    }
}

size_t defaultStrategy(const Thread& thrd, const ThreadManager& thrdMngr) {
    return 1;
}