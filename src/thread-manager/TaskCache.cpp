#include "TaskCache.h"

const TaskCache::QueuePtr& TaskCache::getQueuePtr() {
    int minPoint = 9999;
    int idx = 0, cnt = 0;
    for (auto& elem: _M_queues) {
        if (elem.use_count() < minPoint) {
            minPoint = elem.use_count();
            idx = cnt;
        }
        cnt++;
    }
    return _M_queues[idx];
}