#include "Task.h"
#include "Queue.h"
#include <iostream>
#include <functional>
#include <thread>
using namespace std;

LockFreeQueue<Task> que(1005);
Task taskArray[1000];

// 待执行的目标函数
void printNr(int a) {
    cout << a << endl;
}

void push() {
    for (int i = 0; i < 1000; i++) {
        packaged_task<void()> newTask(bind(printNr, i));
        taskArray[i] = Task(std::move(newTask));
    }
}

void trans() {
    que.push(taskArray, 1000);
}

void exe() {
    que.pop(taskArray, 1000);
    for (int i = 0; i < 1000; i++) {
        Task t = taskArray[i];
        t();
    }
}

int main() {
    thread th;
    // 填充所有任务
    th = thread(push);
    th.join();
    // 消耗所有任务
    th = thread(exe);
    th.join();

    return 0;
}