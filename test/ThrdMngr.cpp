#include <iostream>
#include "Thread.h"
#include <unistd.h>
#include <chrono>
using namespace std;
/**
 * 本代码主要用来测试现有线程池效率，当前硬件条件为：
 *  CPU：GenuineIntel x86_64 2.4GHz
 *  MEM: 2G no swappiness
 * 因此，我们可以做大致推测，假设每条指令流水线的情况下
 * 平均使用 2 个时钟周期；则 2.4GHz 则最多可以执行 1.2 G
 * 条指令，假设 C++ 每条语句平均需要 10 条汇编指令，则
 * 每秒最多可以执行 1e8 条语句。
 * 一秒内顺序执行 100 μs 的任务，则最多执行不到 1w 条。
 */
atomic_int cnt {0};
void func(int a) {
    usleep(100);
    cnt++;
}

void testNonThread() {
    auto start = chrono::steady_clock::now();
    for (int i = 0; i < 10000; i++) func(i);
    auto endt = chrono::steady_clock::now();
    int cost = chrono::duration_cast<chrono::milliseconds>(endt - start).count();
    printf("%d tasks finished in %d ms (%d TPS) without multi-threads.\n", cnt.load(), cost, cnt.load() * 1000 / cost);
    cnt = 0;
}

int main() {
    ThreadManager thrdMngr;
    testNonThread();
    auto start = chrono::steady_clock::now();
    for (int i = 0; i < 50000; i++) {
        auto ft = thrdMngr.submit(func, i);
        while (!ft.valid()) {
            this_thread::yield();
            ft = thrdMngr.submit(func, i);
        }
    }
    auto endt = chrono::steady_clock::now();
    int cost = chrono::duration_cast<chrono::milliseconds>(endt - start).count();
    printf("%d tasks finished in %d ms (%d TPS) with multi-threads.\n", cnt.load(), cost, cnt.load() * 1000 / cost);
    return 0;
}