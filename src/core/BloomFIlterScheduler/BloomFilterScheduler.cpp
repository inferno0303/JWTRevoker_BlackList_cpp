#include "BloomFilterScheduler.h"

SCHEDULER::BloomFilterScheduler::BloomFilterScheduler(): bloom_filters(initBloomFilters()) {
}

BFS::BloomFilters SCHEDULER::BloomFilterScheduler::initBloomFilters() {
    return BFS::BloomFilters(3600 * 24, 1, 8 * 1024, 5);
}

BFS::BloomFilters SCHEDULER::BloomFilterScheduler::getBloomFilters() {
    return this->bloom_filters;
}
