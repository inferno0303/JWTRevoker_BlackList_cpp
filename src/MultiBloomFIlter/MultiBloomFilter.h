// MultiBloomFilter.h
#ifndef MULTI_BLOOM_FILTER_H
#define MULTI_BLOOM_FILTER_H

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include "../BloomFilter/BloomFilter.h"

namespace MBF
{

    class MultiBloomFilter
    {
    private:
        // `MAX_JWT_LIFETIME`：Maximum validity period of the JWT (seconds)
        time_t MAX_JWT_LIFETIME = 0;

        // `FILTER_ROTATION_TIME`：Cycle rotation speed of the Bloom filter (seconds)
        time_t FILTER_ROTATION_TIME = 0;

        // `BLOOMFILTER_SIZE`：The size of bloom filter
        size_t BLOOMFILTER_SIZE = 0;

        // `HASH_FUNCTION_NUM`：The number of hash function
        unsigned int HASH_FUNCTION_NUM = 0;

        // `NUM_FILTERS`：Calculate the number of bloom filter
        unsigned int NUM_FILTERS = 0;

        // 用于存储 BloomFilter 对象的向量
        std::vector<BF::BloomFilter> filters;

    public:
        explicit MultiBloomFilter(time_t MAX_JWT_LIFETIME,
                                  time_t FILTER_ROTATION_TIME,
                                  size_t BLOOMFILTER_SIZE,
                                  unsigned int HASH_FUNCTION_NUM);

        void revoke_jwt(const std::string &jwt_token, time_t exp_time);

        bool is_jwt_revoked(const std::string &jwt_token, time_t exp_time);

        void rotate_filters();
    };

} // namespace MBF

#endif // MULTI_BLOOM_FILTER_H
