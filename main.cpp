#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <random>
#include "thread_pool.h"
#include "ThreadPool.h"

using namespace std;

void test() {
    this_thread::sleep_for(chrono::microseconds(1));
}

void test1() {
    this_thread::sleep_for(chrono::microseconds(1));
}

void thread_pool_test() {
    for (int n = 900; n <= 90000; n *= 10) {
        cout << n << " missions: ";
        ThreadPool Pool(3);
        Pool.init();
        auto start_time = std::chrono::system_clock::now();
        for (int i = 0; i < n; i++) {
            Pool.submit(test);
        }
        Pool.shutdown();
        auto cost_time = std::chrono::duration_cast<chrono::milliseconds>(
                std::chrono::system_clock::now() - start_time).count();
        cout << "ThreadPool cost = " << cost_time << " ms, " << std::flush;

        thread_pool pool;
        pool.start();
        start_time = std::chrono::system_clock::now();
        for (int i = 0; i < n; i++) {
            pool.submit(test1);
        }
        pool.shutdown();
        cost_time = std::chrono::duration_cast<chrono::milliseconds>(
                std::chrono::system_clock::now() - start_time).count();
        cout << "thread_pool cost = " << cost_time << " ms." << endl;
    }
}

int main(int argc, char *argv[]) {
    thread_pool_test();

    return 0;
}