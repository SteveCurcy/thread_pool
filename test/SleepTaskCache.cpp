#include "Thread.h"
#include "TaskCache.h"
#include <iostream>
#include <cstdio>
#include <chrono>
#include <unistd.h>
#include <random>
using namespace std;

random_device rd;
mt19937 gen(rd());
uniform_int_distribution<> disb(100, 300);
atomic_int cnt{0};
void print(int a) { usleep(disb(gen)); cnt++; }

void epoch(int nr) {
    Thread threads[10];
    TaskCache taskCache;
    for (int i = 0; i < 10; i++) {
        threads[i].start(taskCache.getQueuePtr());
    }
    printf("[TEST] %d tasks to execute....\n", nr);
    auto start = chrono::steady_clock::now();
    for (int i = 0; i < nr; i++) {
        while (!taskCache.submitNonBlock(print, i).valid()) this_thread::yield();
    }
    while (cnt.load(std::memory_order::memory_order_consume) < nr) this_thread::yield();
    auto cost = chrono::duration_cast<chrono::milliseconds>(
        chrono::steady_clock::now() - start).count();
    cout << nr << " tasks cost " << cost << "ms" << endl;
    cout << "TPS (Tasks/s) is " << 1000 * nr / cost << endl;
    printf("[TEST] %d tasks finished....\n", nr);
}

int main(int argc, char* argv[]) {
    int nr = 500000;
    if (argc > 1) nr = atoi(argv[1]);
    epoch(nr);
    return 0;
}