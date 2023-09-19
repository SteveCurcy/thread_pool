#ifndef LOCK_FREE_QUEUQ_H
#define LOCK_FREE_QUEUQ_H

#include <sys/types.h>
#include <atomic>
#include <thread>
#include <vector>
#include <algorithm>

constexpr size_t LOCK_FREE_QUEUE_DEFAULT_SIZE = 1000;
/**
 * 当前的类是一个环形队列，可以看作一个 ringbuffer，其长度在初始化时固定；
 * 申请空间大小不可改变，因此是定长的队列，通过 CAS 机制保证其无锁操作
 */
template<typename _TyData>
class LockFreeQueue final {
    _TyData *_M_queue;
    std::atomic_ulong _M_read,      // 下一个可读性的位置
                      _M_readable,  // 最后一个可读元素的下一个位置
                      _M_write,     // 下一个可以写入的位置
                      _M_writeable, // 最后一个可写元素的下一个位置
                      _M_size;      // 队列中的元素数量
    size_t _M_allocSize;
    std::atomic_ulong _M_pushd;     // 存储当前时间段内添加的元素数量
    std::atomic_ulong _M_popd;      // 存储当前时间段内取出的元素数量
public:
    LockFreeQueue(size_t _size = LOCK_FREE_QUEUE_DEFAULT_SIZE): _M_allocSize(_size) {
        _M_read = 0;
        _M_write = 0;
        _M_readable = 0;
        _M_writeable = 0;
        _M_pushd = 0;
        _M_popd = 0;
        _M_queue = new _TyData[_size];
    }
    virtual ~LockFreeQueue() { delete[] _M_queue; }

    size_t push(const _TyData* elems, size_t nr);

    size_t pop(_TyData* elems, size_t nr);

    size_t getPushd() { // 返回当前添加的元素数量，并将当前值置 0
        return _M_pushd.exchange(0, std::memory_order::memory_order_acq_rel);
    }

    size_t getPopd() {  // 返回当前取出的元素数量，并将当前值置 0
        return _M_popd.exchange(0, std::memory_order::memory_order_acq_rel);
    }

    size_t index(size_t pos) const { return pos % _M_allocSize; }

    size_t diff(size_t pre, size_t post) { return (post + _M_allocSize - pre) % _M_allocSize; }

    inline bool full() const {
        return _M_write.load(std::memory_order::memory_order_consume)
        == index(_M_writeable.load(std::memory_order::memory_order_consume) - 1);
    }

    inline bool empty() const {
        return _M_read.load(std::memory_order::memory_order_consume)
        == _M_readable.load(std::memory_order::memory_order_consume);
    }

    inline size_t size() const {
        return _M_size.load(std::memory_order::memory_order_consume);
    }

    inline size_t capicity() const { return _M_allocSize; }
};

template<typename _TyData>
size_t LockFreeQueue<_TyData>::push(const _TyData* elems, size_t nr) {
    size_t currentWriteIndex = _M_write.load(std::memory_order::memory_order_consume);
    size_t currentWriteableIndex;
    size_t actualNr = nr;

    do {
        currentWriteableIndex = _M_writeable.load(std::memory_order::memory_order_consume);
        if (currentWriteIndex == index(currentWriteableIndex - 1))
            return 0;   // 如果队列满了，返回 0，无法写入
        actualNr = std::min(nr, diff(currentWriteIndex, currentWriteableIndex - 1));
    } while (!_M_write.compare_exchange_strong(
        currentWriteIndex, index(currentWriteIndex + actualNr),
        std::memory_order::memory_order_acq_rel
    )); // 获取可以写入的位置

    // 将数据写入到对应位置
    for (size_t i = 0; i < actualNr; i++) {
        _M_queue[index(currentWriteIndex + i)] = elems[i];
    }

    // 更新可读的最终位置
    size_t expectReadable = currentWriteIndex;
    size_t desiredReadable = index(currentWriteIndex + actualNr);
    while (!_M_readable.compare_exchange_strong(
        expectReadable, desiredReadable,
        std::memory_order::memory_order_acq_rel
    )) {
        expectReadable = currentWriteIndex;
        std::this_thread::yield();  // 更新失败，则暂时让出 CPU 一段时间
    }

    _M_size += actualNr;
    _M_pushd += actualNr;
    return actualNr;
}

template<typename _TyData>
size_t LockFreeQueue<_TyData>::pop(_TyData* elems, size_t nr) {
    size_t currentReadIndex = _M_read.load(std::memory_order::memory_order_consume);
    size_t currentReadableIndex;
    size_t actualNr = nr;

    do {
        currentReadableIndex = _M_readable.load(std::memory_order::memory_order_consume);
        if (currentReadIndex == currentReadableIndex)
            return 0;   // 如果队列为空，返回 0，无数据可读
        actualNr = std::min(nr, diff(currentReadIndex, currentReadableIndex));
    } while (!_M_read.compare_exchange_strong(
        currentReadIndex, index(currentReadIndex + actualNr),
        std::memory_order::memory_order_acq_rel
    )); // 获取可以读取的位置

    // 将数据写入到对应位置
    for (size_t i = 0; i < actualNr; i++) {
        elems[i] = _M_queue[index(currentReadIndex + i)];
    }

    // 更新可写的最终位置
    size_t expectWriteable = currentReadIndex;
    size_t desiredWriteable = index(currentReadIndex + actualNr);
    while (!_M_writeable.compare_exchange_strong(
        expectWriteable, desiredWriteable,
        std::memory_order::memory_order_acq_rel
    )) {
        expectWriteable = currentReadIndex;
        std::this_thread::yield();  // 更新失败，则暂时让出 CPU 一段时间
    }

    _M_size -= actualNr;
    _M_popd += actualNr;
    return actualNr;
}

/**
 * 变长无锁队列，当需要扩容队列的时候，申请一个新的静态无锁队列；
 * 然后将写任务指向新队列，而原队列只负责读；
 * 当读队列空时，完成最终的指针修正
 */
template<typename _TyData>
class DynamicLockFreeQueue final {
    LockFreeQueue<_TyData> *curPtr,
                           *newPtr;
    LockFreeQueue<_TyData> *readPtr,
                           *writePtr;
public:
    DynamicLockFreeQueue(size_t _size = LOCK_FREE_QUEUE_DEFAULT_SIZE):
                         curPtr(new LockFreeQueue<_TyData>(_size)),
                         newPtr(nullptr) {
        readPtr = curPtr;
        writePtr = curPtr;
    }
    ~DynamicLockFreeQueue() {
        readPtr = writePtr = nullptr;
        delete curPtr;
        delete newPtr;
    }

    size_t push(const _TyData* elems, size_t nr = 1) { return writePtr->push(elems, nr); }

    size_t pop(_TyData* elems, size_t nr = 1) { return readPtr->pop(elems, nr); }

    size_t getPushd() { return writePtr->getPushd(); }

    size_t getPopd() { return readPtr->getPopd(); }

    /*
     * 这个函数只能由控制者使用，其他线程不能调用
     * 此外，这个大小修改只适用于线程池，因为队列中的元素不断被消耗；
     * 导致队列最终都一定为空，从而能够进行替换
     */
    bool resize(size_t _size) {
        if (_size < ((curPtr->capicity() * 3) >> 1) || newPtr) return false;
        newPtr = new LockFreeQueue<_TyData>(_size);
        writePtr = newPtr;
        while (curPtr->size()) { std::this_thread::yield(); }
        /**
         * 现在旧队列为空，修改指针，完成扩容；
         * 1. 首先将读指针指向新队列，保证读操作的进行；
         * 2. 然后释放原队列空间，保证释放过程和读写过程异步；
         * 3. 释放完成后，修正当前队列指针
         */
        readPtr = newPtr;   // 1. redirect the reading pointer
        delete curPtr;      // 2. release the old queue space
        curPtr = newPtr;    // 3. correct the current pointer
        newPtr = nullptr;
        return true;
    }

    inline bool full() const { return curPtr->size(); }

    inline bool empty() const { return curPtr->size(); }

    inline size_t size() const { return curPtr->size(); }

    inline size_t capicity() const { return curPtr->capicity(); }
};

#endif