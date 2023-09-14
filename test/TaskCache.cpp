#include "Thread.h"
#include "TaskCache.h"
#include <iostream>
#include <chrono>
#include <unistd.h>
#include <random>
using namespace std;

random_device rd;
mt19937 gen(rd());
uniform_int_distribution<> disb(100, 300);
atomic_int cnt{0};
void print(int a) { usleep(disb(gen)); cnt++; }
#define TOT_TASK 500000

int main() {
    Thread threads[10];
    TaskCache taskCache;
    for (int i = 0; i < 10; i++) {
        threads[i].start(taskCache.getQueuePtr());
    }
    auto start = chrono::steady_clock::now();
    for (int i = 0; i < TOT_TASK; i++) {
        while (!taskCache.submitNonBlock(print, i).valid()) this_thread::yield();
    }
    while (cnt.load(std::memory_order::memory_order_consume) < TOT_TASK) this_thread::yield();
    auto cost = chrono::duration_cast<chrono::milliseconds>(
        chrono::steady_clock::now() - start).count();
    cout << TOT_TASK << " tasks cost " << cost << "ms" << endl;
    cout << "TPS (Tasks/s) is " << 1000 * TOT_TASK / cost << endl;
    return 0;
}