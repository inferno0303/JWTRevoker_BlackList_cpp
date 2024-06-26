// MultiBloomFilter.h
#ifndef BLOOM_FILTER_MANAGER_H
#define BLOOM_FILTER_MANAGER_H

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include "../BloomFilter/BloomFilter.h"

namespace BFM {

    class BloomFilterManager {
    private:
        // `MAX_JWT_LIFETIME`：Maximum validity period of the JWT (seconds)
        time_t MAX_JWT_LIFETIME;

        // `BLOOM_FILTER_ROTATION_TIME`：Cycle rotation speed of the Bloom filter (seconds)
        time_t BLOOM_FILTER_ROTATION_TIME;

        // `BLOOM_FILTER_SIZE`：The size of bloom filter
        size_t BLOOM_FILTER_SIZE;

        // `NUM_HASH_FUNCTION`：The number of hash function
        unsigned int NUM_HASH_FUNCTION;

        // `NUM_BLOOM_FILTER`：Calculate the number of bloom filter
        unsigned int NUM_BLOOM_FILTER;

        // 用于存储多个 BloomFilter 对象的向量
        std::vector<BF::BloomFilter> filters;

    public:
        explicit BloomFilterManager(time_t maxJwtLifetime, time_t bloomFilterRotationTime, size_t bloomFilterSize,
                           unsigned int numHashFunction);

        // 撤回JWT
        void jwt_revoke(const std::string &jwt_token, time_t exp_time);

        // 检查JWT
        bool is_jwt_revoke(const std::string &jwt_token, time_t exp_time);

        // 获取轮换时间
        time_t getBLOOM_FILTER_ROTATION_TIME() const;

        void rotate_filters();
    };

} // namespace BFM

#endif // BLOOM_FILTER_MANAGER_H
