#include "Thread.h"
#include <unistd.h>
#include <iostream>
#include <chrono>
using namespace std;

constexpr int turn = 1000000;

atomic_int cnt;
int bucket[turn];

void printNr(int a)
{
    cnt++;
    bucket[a] = a * a;
}

int main()
{
    ThreadManager mngr(2);
    long long costTime = 0LL;
    mngr.start();

    for (int i = 0; i < turn; i++)
    {
        auto startTime = chrono::system_clock::now();

        std::future<void> ret = mngr.submit(printNr, i);

        auto endTime = chrono::system_clock::now();
        auto duration = chrono::duration_cast<chrono::microseconds>(endTime - startTime);
        costTime += duration.count();
    }
    mngr.shutdown();

    cout << "[INFO] TestThread: Spent " << ((double)costTime / turn) << " us/task." << endl;

    return cnt.load(std::memory_order_consume) - turn;
}