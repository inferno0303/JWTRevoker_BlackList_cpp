#include "BloomFilterManager.h"

BFM::BloomFilterManager::BloomFilterManager(time_t maxJwtLifetime, time_t bloomFilterRotationTime,
                                            size_t bloomFilterSize, unsigned int numHashFunction) {
    if (maxJwtLifetime == 0) {
        throw std::invalid_argument("MAX_JWT_LIFETIME cannot be 0");
    }
    MAX_JWT_LIFETIME = maxJwtLifetime;

    if (bloomFilterRotationTime == 0) {
        throw std::invalid_argument("BLOOM_FILTER_ROTATION_TIME cannot be 0");
    }
    this->BLOOM_FILTER_ROTATION_TIME = bloomFilterRotationTime;

    if (bloomFilterSize == 0) {
        throw std::invalid_argument("BLOOM_FILTER_SIZE cannot be 0");
    }
    this->BLOOM_FILTER_SIZE = bloomFilterSize;

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

void BFM::BloomFilterManager::jwt_revoke(const std::string &jwt_token, time_t exp_time) {
    time_t remaining_time = exp_time - time(nullptr);
    int num_filters = std::ceil(remaining_time / BLOOM_FILTER_ROTATION_TIME);
    for (int i = 0; i < num_filters; ++i) {
        filters[i].add(jwt_token);
    }
}

bool BFM::BloomFilterManager::is_jwt_revoke(const std::string &jwt_token, time_t exp_time) {
    time_t remaining_time = exp_time - time(nullptr);
    int num_filters = std::ceil(remaining_time / BLOOM_FILTER_ROTATION_TIME);
    for (int i = 0; i < num_filters; ++i) {
        if (filters[i].contains(jwt_token)) {
            return true;
        }
    }
    return false;
}

void BFM::BloomFilterManager::rotate_filters() {
    filters.erase(filters.begin());
    filters.emplace_back(BLOOM_FILTER_SIZE, NUM_HASH_FUNCTION);
}

time_t BFM::BloomFilterManager::getBLOOM_FILTER_ROTATION_TIME() const {
    return BLOOM_FILTER_ROTATION_TIME;
}
