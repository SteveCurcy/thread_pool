#include "Thread.h"
#include <unistd.h>
#include <iostream>
#include <chrono>
using namespace std;

constexpr int turn = 1000000;

atomic_int cnt;
long long bucket[turn];

void printNr(int a)
{
    cnt++;
    bucket[a] = a * a;
}

int main()
{
    LockFreeQueue<Task> que;
    constexpr int Nr = 2;
    Thread ths[Nr];
    for (int i = 0; i < Nr; i++) {
        ths[i].setQue(&que);
        ths[i].start();
    }
    auto startTime = chrono::system_clock::now();
    for (int i = 0; i < turn; i++) {
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

    auto endTime = chrono::system_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
    cout <<  "[INFO] TestThread: Spent " << duration.count() << " ms." << endl;

    return cnt.load(std::memory_order_consume) - turn;
}