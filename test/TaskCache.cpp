#include "Thread.h"
#include "TaskCache.h"
#include <iostream>
using namespace std;

void print() { cout << 1 << endl; }

int main() {
    Thread threads[10];
    TaskCache taskCache;
    for (int i = 0; i < 10; i++) {
        threads[i].start(taskCache.getQueuePtr());
    }
    for (int i = 0; i < 10000; i++) {
        while (!taskCache.submitNonBlock(print).valid()) this_thread::yield();
    }
    return 0;
}