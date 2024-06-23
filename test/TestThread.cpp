#include "Thread.h"
#include <unistd.h>
#include <iostream>
using namespace std;

atomic_int cnt;
long long bucket[100000];

void printNr(int a)
{
    cnt++;
    bucket[a] = a * a;
}

int main()
{
    LockFreeQueue<Task> que(1000);
    constexpr int Nr = (1 << 1);
    Thread ths[Nr];
    for (int i = 0; i < Nr; i++) {
        ths[i].setQue(&que);
        ths[i].start();
    }
    for (int i = 0; i < 100000; i++) {
        packaged_task<void()> ptask = packaged_task<void()>(
            std::bind(printNr, i)
        );
        Task task(std::move(ptask));
        while (!que.push(&task, 1)) {
            this_thread::yield();
        }
    }
    // 等队列为空，所有线程执行任务完成才能退出
    while (!que.empty()) {
        this_thread::yield();
    }
    for (int i = 0; i < Nr; i++) {
        ths[i].shutdown();
    }
    return cnt.load(std::memory_order_consume) - 100000;
}