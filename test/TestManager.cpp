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
    mngr.start();

    auto startTime = chrono::system_clock::now();
    for (int i = 0; i < turn; i++)
    {
        std::future<void> ret = mngr.submit(printNr, i);
    }
    mngr.shutdown();

    auto endTime = chrono::system_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
    cout <<  "[INFO] TestThread: Spent " << duration.count() << " ms." << endl;

    return cnt.load(std::memory_order_consume) - turn;
}