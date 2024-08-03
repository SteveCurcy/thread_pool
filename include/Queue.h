#ifndef QUEUQ_H
#define QUEUQ_H

#include <sys/types.h>
#include <atomic>
#include <thread>
#include <vector>
#include <algorithm>

constexpr size_t QUEUE_DEFAULT_SIZE = 1000;

/**
 * @template _TyData
 * @class Queue
 * @brief 队列的抽象基类模板
 *
 * Queue 类是一个模板类，用于定义队列的抽象接口。它提供了队列的基本操作，如入队（push）、
 * 出队（pop）、获取队列大小（size）、容量（capacity）、判断队列是否已满（full）和
 * 是否为空（empty）的接口。这些接口是纯虚函数，需要由派生类具体实现。
 *
 * 队列是一种先进先出（FIFO）的数据结构，它允许在队列的一端（队尾）添加元素，在另一端
 * （队头）移除元素。
 *
 * @note 由于这个类是抽象基类，因此不能直接实例化。必须通过继承这个类并实现其所有纯虚函数
 * 来创建具体的队列类。
 */
template <typename _TyData>
class Queue
{
public:
    virtual ~Queue(){};
    virtual size_t push(const _TyData *, size_t) = 0;
    virtual size_t pop(_TyData *, size_t) = 0;
    virtual size_t size() const = 0;
    virtual size_t capacity() const = 0;
    virtual bool full() const = 0;
    virtual bool empty() const = 0;
};

/**
 * @template _TyData 队列中存储的数据类型。
 * @class LockFreeQueue
 * @brief 无锁队列类，继承自 Queue 模板类。
 *
 * 提供了一个线程安全的无锁队列实现，使用原子操作来管理读写位置。
 * 当前的类是一个环形队列，可以看作一个 ringbuffer，其长度在初始化时固定；
 * 申请空间大小不可改变，因此是定长的队列，通过 CAS 机制保证其无锁操作;
 * 空余一个位置用于区分队列满/空
 *
 */
template <typename _TyData>
class LockFreeQueue final : public Queue<_TyData>
{
    _TyData *_M_queue;
    alignas(8)
        std::atomic_ulong _M_read, // 下一个可读性的位置
        _M_readable,               // 最后一个可读元素的下一个位置
        _M_write,                  // 下一个可以写入的位置
        _M_writeable;              // 最后一个可写元素的下一个位置
    size_t _M_allocSize;
    size_t index(size_t pos) const { return pos % _M_allocSize; }
    size_t diff(size_t pre, size_t post) const { return (post + _M_allocSize - pre) % _M_allocSize; }

public:
    LockFreeQueue(size_t _size = QUEUE_DEFAULT_SIZE) : _M_allocSize(_size)
    {
        _M_read = 0;
        _M_write = 0;
        _M_readable = 0;
        _M_writeable = _M_allocSize - 1;
        _M_queue = new _TyData[_size];
    }
    virtual ~LockFreeQueue() { delete[] _M_queue; }

    size_t push(const _TyData *elems, size_t nr) override;

    size_t pop(_TyData *elems, size_t nr) override;

    float getStress()
    {
        return (float)(size() + 1) / _M_allocSize;
    }

    bool full() const override
    {
        return _M_write.load(std::memory_order::memory_order_consume) ==
               _M_writeable.load(std::memory_order::memory_order_consume);
    }

    bool empty() const override
    {
        return _M_read.load(std::memory_order::memory_order_consume) ==
               _M_readable.load(std::memory_order::memory_order_consume);
    }

    size_t size() const override
    {
        return diff(_M_read.load(std::memory_order_consume),
                    _M_readable.load(std::memory_order_consume));
    }

    size_t capacity() const override { return _M_allocSize; }
};

template <typename _TyData>
size_t LockFreeQueue<_TyData>::push(const _TyData *elems, size_t nr)
{
    size_t currentWriteIndex = _M_write.load(std::memory_order::memory_order_consume);
    size_t currentWriteableIndex;
    size_t actualNr = nr;

    do
    {
        currentWriteableIndex = _M_writeable.load(std::memory_order::memory_order_consume);
        if (currentWriteIndex == currentWriteableIndex)
            return 0; // 如果队列满了，返回 0，无法写入
        actualNr = std::min(nr, diff(currentWriteIndex, currentWriteableIndex));
    } while (!_M_write.compare_exchange_strong(
        currentWriteIndex, index(currentWriteIndex + actualNr),
        std::memory_order::memory_order_acq_rel)); // 获取可以写入的位置

    // 将数据写入到对应位置
    for (size_t i = 0; i < actualNr; i++)
    {
        _M_queue[index(currentWriteIndex + i)] = elems[i];
    }

    // 更新可读的最终位置
    size_t expectReadable = currentWriteIndex;
    size_t desiredReadable = index(expectReadable + actualNr);
    while (!_M_readable.compare_exchange_strong(
        expectReadable, desiredReadable,
        std::memory_order::memory_order_acq_rel))
    {
        expectReadable = currentWriteIndex;
        std::this_thread::yield(); // 更新失败，则暂时让出 CPU 一段时间
    }

    return actualNr;
}

template <typename _TyData>
size_t LockFreeQueue<_TyData>::pop(_TyData *elems, size_t nr)
{
    size_t currentReadIndex = _M_read.load(std::memory_order::memory_order_consume);
    size_t currentReadableIndex;
    size_t actualNr = nr;

    do
    {
        currentReadableIndex = _M_readable.load(std::memory_order::memory_order_consume);
        if (currentReadIndex == currentReadableIndex)
            return 0; // 如果队列为空，返回 0，无数据可读
        actualNr = std::min(nr, diff(currentReadIndex, currentReadableIndex));
    } while (!_M_read.compare_exchange_strong(
        currentReadIndex, index(currentReadIndex + actualNr),
        std::memory_order::memory_order_acq_rel)); // 获取可以读取的位置

    // 将数据写入到对应位置
    for (size_t i = 0; i < actualNr; i++)
    {
        elems[i] = _M_queue[index(currentReadIndex + i)];
    }

    // 更新可写的最终位置
    size_t expectWriteable = index(currentReadIndex + _M_allocSize - 1);
    size_t stashWriteable = expectWriteable;
    size_t desiredWriteable = index(expectWriteable + actualNr);
    while (!_M_writeable.compare_exchange_strong(
        expectWriteable, desiredWriteable,
        std::memory_order::memory_order_acq_rel))
    {
        expectWriteable = stashWriteable;
        std::this_thread::yield(); // 更新失败，则暂时让出 CPU 一段时间
    }

    return actualNr;
}

/**
 * 变长队列，当需要扩容队列的时候，申请一个新的定长队列；
 * 然后将写任务指向新队列，而原队列只负责读；
 * 当读队列空时，完成最终的指针修正，你可以选择定长队列的基础实现
 */
template <typename _TyData, typename _BaseQueue = LockFreeQueue<_TyData>>
class DynamicQueue final : public Queue<_TyData>
{
    _BaseQueue *curPtr, *newPtr,
        *readPtr, *writePtr;

public:
    DynamicQueue(size_t _size = QUEUE_DEFAULT_SIZE) : curPtr(new _BaseQueue(_size)),
                                                      newPtr(nullptr)
    {
        readPtr = curPtr;
        writePtr = curPtr;
    }
    ~DynamicQueue()
    {
        readPtr = writePtr = nullptr;
        delete curPtr;
        delete newPtr;
    }

    size_t push(const _TyData *elems, size_t nr = 1) override { return writePtr->push(elems, nr); }

    size_t pop(_TyData *elems, size_t nr = 1) override { return readPtr->pop(elems, nr); }

    /*
     * 这个函数只能由控制者使用，其他线程不能调用
     * 此外，这个大小修改只适用于线程池，因为队列中的元素不断被消耗；
     * 导致队列最终都一定为空，从而能够进行替换
     */
    bool resize(size_t _size)
    {
        if (_size < ((curPtr->capicity() * 3) >> 1) || newPtr)
            return false;
        newPtr = new LockFreeQueue<_TyData>(_size);
        writePtr = newPtr;
        while (curPtr->size())
        {
            std::this_thread::yield();
        }
        /**
         * 现在旧队列为空，修改指针，完成扩容；
         * 1. 首先将读指针指向新队列，保证读操作的进行；
         * 2. 然后释放原队列空间，保证释放过程和读写过程异步；
         * 3. 释放完成后，修正当前队列指针
         */
        readPtr = newPtr; // 1. redirect the reading pointer
        delete curPtr;    // 2. release the old queue space
        curPtr = newPtr;  // 3. correct the current pointer
        newPtr = nullptr;
        return true;
    }

    bool empty() const override { return curPtr->empty(); }

    size_t size() const override { return curPtr->size(); }

    size_t capacity() const override { return curPtr->capacity(); }
};

#endif