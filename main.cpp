#include <thread>
#include <chrono>
#include <random>
#include <cstring>
#include <iostream>
#include "thread_pool.h"

using namespace std;
random_device rd;
mt19937 gen(rd());
uniform_int_distribution<> dist(100, 500);

// atomic_int cnt(0);

void test() {
    // cnt++;
    auto sleepTime = dist(gen);
    this_thread::sleep_for(chrono::nanoseconds(sleepTime));
}

void thread_pool_test(int n) {
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
    cout << "TPS (Tasks/s): " << n / (1.0 * cost_time / 1000) << endl;
}

int main(int argc, char *argv[]) {
    int n = 50000;
    if (argc > 1) {
        int argId = 1;
        while (argId < argc) {
            int len = strlen(argv[argId]);
            int off = 0;
            if (len > 2 && argv[argId][0] == '-' && argv[argId][1] == '-') off = 2;
            else if (len > 1 && argv[argId][0] == '-') off = 1;
            switch (argv[argId][off])
            {
            case 'n':
                argId++;
                n = atoi(argv[argId]);
                argId++;
                break;
            case 'h':
                printf("Usage: ./main [-n CNT] [-h]\n\n\
-n\tthe number of tasks you wanna test. it's set 50k by default.\n\
-h\tshow this help message.\n");
                exit(0);
                break;
            default:
                printf("Error: wrong option keys! see more help by command './main -h'.\n");
                exit(0);
                break;
            }
        }
    }
    thread_pool_test(n);
    // cout << "real tasks running count : " << cnt << endl;
    return 0;
}