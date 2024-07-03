#ifndef BLOOMFILTERSCHEDULER_H
#define BLOOMFILTERSCHEDULER_H

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <winsock2.h>
#include <vector>
#include "../BloomFIlters/BloomFilters.h"

namespace SCHEDULER {
    class BloomFilterScheduler {
    private:
        BFS::BloomFilters bloom_filters;

        static BFS::BloomFilters initBloomFilters();
    public:
        BloomFilterScheduler();
        BFS::BloomFilters getBloomFilters();
    };
} // namespace SCHEDULER

#endif // BLOOMFILTERSCHEDULER_H
