# thread_pool
[![](https://img.shields.io/badge/Author-Xu.Cao-lightgreen)](https://github.com/SteveCurcy) ![](https://img.shields.io/badge/Version-2.1.2-yellow)

基于 C++ 实现的线程池，简单但高效。

## 内容列表

- [1. 背景 :cookie:](#1-背景-cookie)
- [2. 解决方案 :candy:](#2-解决方案-candy)
- [3. 安装和运行 :birthday:](#3-安装和运行-birthday)
- [4. 现有问题 :sandwich:](#4-现有问题-sandwich)
- [5. 使用许可 :page_facing_up:](#5-使用许可-page_facing_up)

## 1. 背景 :cookie:
本项目主要想要完成一个开箱即用的线程池库，在尽可能简单易用、方便定制的情况下实现高性能线程池。

## 2. 解决方案 :candy:
目前项目的特征主要有：

- 无锁化队列：使用「CAS 机制」实现了队列的无锁化，可以实现轻量级元素添加、弹出，批量添加和弹出，避免了互斥锁带来的高额开销。
- 自旋锁：使用「原子类型」实现了自旋锁，在短期加锁、解锁过程中替代「互斥锁」和「条件变量」，从而提高项目性能。
- 暂停和恢复：可以暂停/恢复线程池的任务执行（已经在执行的任务无法暂停），使用「条件变量」实现，因此高频率暂停/恢复会带来较大的开销。

在现有的线程池项目中，线程池测试代码 TestManager 会比线程测试代码 TestThread 开销更高。因为线程池的目的是可以接受「任意函数」作为目标执行函数，使用了「模板函数」作为任务提交的接口，而线程测试代码中使用了「固定的目标执行函数」。因此，TestManager 的执行时间比 TestThread 的更高。

## 3. 安装和运行 :birthday:

C++ Build

```
mkdir build && cd build
cmake ..
make
ctest
```

## 4. 现有问题 :sandwich:
已经不存在任务丢失问题。目前开发环境为 2 核 2G 服务器，因此可以支持的最大任务数量暂时测试为 10w。

## 5. 使用许可 :page_facing_up:
[GPL 2.0](./LICENSE) &copy; Xu.Cao (Steve Curcy)
