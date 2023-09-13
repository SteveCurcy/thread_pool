#include "Thread.h"
#include "TaskCache.h"
#include <iostream>
using namespace std;

void print(int a) { cout << a << endl; }

int main() {
    Thread threads[10];
    TaskCache taskCache;
    for (int i = 0; i < 10; i++) {
        threads[i].start(taskCache.getQueuePtr());
    }
    for (int i = 0; i < 10000; i++) {
        while (!taskCache.submitNonBlock(print, i).valid()) this_thread::yield();
    }
    return 0;
}