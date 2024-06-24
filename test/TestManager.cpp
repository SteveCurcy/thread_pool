#include "Thread.h"
#include <unistd.h>
#include <iostream>
using namespace std;

atomic_int cnt;
int bucket[100000];

void printNr(int a)
{
    cnt++;
    bucket[a] = a * a;
}

int main()
{
    ThreadManager mngr;
    mngr.start();
    for (int i = 0; i < 100000; i++)
    {
        std::future<void> ret;
        while (!ret.valid())
        {
            ret = mngr.submit(printNr, i);
        }
    }
    mngr.shutdown();
    return cnt.load(std::memory_order_consume) - 100000;
}