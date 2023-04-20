#include <iostream>
#include <thread>
#include <chrono>
#include "thread_pool.h"

using namespace std;

void test() {
    this_thread::sleep_for(chrono::microseconds(1));
}

void thread_pool_test() {
    for (int n = 5000000; n <= 5000000; n *= 10) {
        cout << n << " missions: ";

        thread_pool pool;
        pool.start();
        auto start_time = std::chrono::system_clock::now();
        for (int i = 0; i < n; i++) {
            pool.submit(test);
        }
        pool.shutdown();
        auto cost_time = std::chrono::duration_cast<chrono::milliseconds>(
                std::chrono::system_clock::now() - start_time).count();
        cout << "thread_pool cost = " << cost_time << " ms." << endl;
    }
}

int main(int argc, char *argv[]) {
    thread_pool_test();

    return 0;
}