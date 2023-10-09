#include <iostream>
#include "Thread.h"
using namespace std;

void func(int a) {
    cout << a << endl;
}

int main() {
    ThreadManager thrdMngr;
    for (int i = 0; i < 10; i++) {
        thrdMngr.submit(func, i);
    }
    return 0;
}