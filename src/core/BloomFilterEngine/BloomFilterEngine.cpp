#include <cmath>
#include "BloomFilterEngine.h"

BloomFilterEngine::BloomFilterEngine(const time_t maxJwtLifetime, const time_t bloomFilterRotationTime,
                                     const size_t bloomFilterSize, const unsigned int numHashFunction) {
    if (maxJwtLifetime == 0) {
        throw std::invalid_argument("MAX_JWT_LIFETIME cannot be 0");
    }
    MAX_JWT_LIFETIME = maxJwtLifetime;

    if (bloomFilterRotationTime == 0) {
        throw std::invalid_argument("BLOOM_FILTER_ROTATION_TIME cannot be 0");
    }
    BLOOM_FILTER_ROTATION_TIME = bloomFilterRotationTime;

    if (bloomFilterSize == 0) {
        throw std::invalid_argument("BLOOM_FILTER_SIZE cannot be 0");
    }
    BLOOM_FILTER_SIZE = bloomFilterSize;

    if (numHashFunction == 0) {
        throw std::invalid_argument("NUM_HASH_FUNCTION cannot be 0");
    }
    NUM_HASH_FUNCTION = numHashFunction;

    // 计算布隆过滤器个数
    NUM_BLOOM_FILTER = std::ceil(MAX_JWT_LIFETIME / BLOOM_FILTER_ROTATION_TIME);

    // 初始化多个 BloomFilter 对象，并存储到向量中
    for (unsigned int i = 0; i < NUM_BLOOM_FILTER; ++i) {
        filters.emplace_back(BLOOM_FILTER_SIZE, NUM_HASH_FUNCTION);
    }
}

void BloomFilterEngine::jwt_revoke(const std::string &jwt_token, time_t exp_time) {
    const time_t remaining_time = exp_time - time(nullptr);
    const int num_filters = std::ceil(remaining_time / BLOOM_FILTER_ROTATION_TIME);
    for (int i = 0; i < num_filters; ++i) {
        filters[i].add(jwt_token);
    }
}

bool BloomFilterEngine::is_jwt_revoke(const std::string &jwt_token, const time_t exp_time) const {
    const time_t remaining_time = exp_time - time(nullptr);
    const int num_filters = std::ceil(remaining_time / BLOOM_FILTER_ROTATION_TIME);
    for (int i = 0; i < num_filters; ++i) {
        if (filters[i].contains(jwt_token)) {
            return true;
        }
    }
    return false;
}

void BloomFilterEngine::rotate_filters() {
    filters.erase(filters.begin());
    filters.emplace_back(BLOOM_FILTER_SIZE, NUM_HASH_FUNCTION);
}

time_t BloomFilterEngine::getBLOOM_FILTER_ROTATION_TIME() const {
    return BLOOM_FILTER_ROTATION_TIME;
}