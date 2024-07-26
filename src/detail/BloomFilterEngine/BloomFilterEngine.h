// BloomFilter.h
#ifndef BLOOMFILTER_ENGINE_H
#define BLOOMFILTER_ENGINE_H

#include <iostream>
#include <vector>
#include <string>
#include "../BloomFilter/BloomFilter.h"

class BloomFilterEngine {
private:
    // `MAX_JWT_LIFETIME`：Maximum validity period of the JWT (seconds)
    unsigned int MAX_JWT_LIFETIME;

    // `BLOOM_FILTER_ROTATION_TIME`：Cycle rotation speed of the Bloom filter (seconds)
    unsigned int BLOOM_FILTER_ROTATION_TIME;

    // `BLOOM_FILTER_SIZE`：The size of bloom filter
    size_t BLOOM_FILTER_SIZE;

    // `HASH_FUNCTION_NUM`：The number of hash function
    unsigned int HASH_FUNCTION_NUM;

    // `BLOOM_FILTER_NUM`：Calculate the number of bloom filter
    unsigned int BLOOM_FILTER_NUM;

    // 用于存储多个 BloomFilter 对象的向量
    std::vector<BF::BloomFilter> filters;

public:
    BloomFilterEngine(unsigned int maxJwtLifetime, unsigned int bloomFilterRotationTime, size_t bloomFilterSize,
                      unsigned int numHashFunction);

    // 撤回JWT
    void jwt_revoke(const std::string &jwt_token, time_t exp_time);

    // 检查JWT
    bool is_jwt_revoke(const std::string &jwt_token, time_t exp_time) const;

    // 轮换布隆过滤器
    void rotate_filters();

    // getter方法
    time_t getMAX_JWT_LIFETIME() const;
    time_t getBLOOM_FILTER_ROTATION_TIME() const;
    unsigned int getBLOOM_FILTER_NUM() const;
    std::vector<unsigned long long> getFILTERS_MSG_NUM() const;
    size_t getBLOOM_FILTER_SIZE() const;
    unsigned int getHASH_FUNCTION_NUM() const;
};

#endif //BLOOMFILTER_ENGINE_H
