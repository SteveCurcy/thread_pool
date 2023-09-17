#include "LockFreeQueue.h"
#include "Task.h"
#include <iostream>
#include <functional>
#include <thread>
using namespace std;
DynamicLockFreeQueue<Task> que;
atomic_int pushd{0}, popd{0};

void func(int a) {}

void ppush() {
    Task x[5];
    for (int i = 0; i < 5; i++) {
        x[i] = Task(packaged_task<void()>(bind(func, i)));
    }
    int nr;
    while ((nr = que.push(x, 5)) == 0) {
        this_thread::yield();
    }
    pushd += nr;
}

void push() {
    for (int i = 0; i < 10000; i += 5) {
        ppush();
    }
}

void pop(int id) {
    for (int i = 0; i < 10000; i += 5) {
        Task x[5];
        int nr;
        while ((nr = que.pop(x, 5)) == 0) {
            this_thread::yield();
        }
        for (int i = 0; i < nr; i++) {
            x[i]();
        }
        popd += nr;
        // cout << "thread " << id << " pushd and popd: " << pushd << ", " << popd << endl;
    }
}

int main() {
    thread t[20];
    for (int i = 0; i < 20; i++) {
        if (i < 10) t[i] = thread(push);
        else t[i] = thread(pop, i - 10);
    }
    for (int i = 0; i < 20; i++) {
        t[i].join();
    }
    if (pushd.load(memory_order::memory_order_consume)
     == popd.load(memory_order::memory_order_consume))
        printf("\033[32m[SUCCESS]\033[0m %d tasks finished...\n", pushd.load());
    else
        printf("\033[31m[ERROR]\033[0m %d tasks missed...\n", pushd - popd);
    return 0;
}