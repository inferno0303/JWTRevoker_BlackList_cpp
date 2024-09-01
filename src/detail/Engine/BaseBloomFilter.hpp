#ifndef BASE_BLOOM_FILTER_HPP
#define BASE_BLOOM_FILTER_HPP

#include <iostream>
#include <vector>
#include <string>

#include "SHA256/SHA256.h"

class BaseBloomFilter {
public:
    BaseBloomFilter(const size_t size, const unsigned int hashFunctionNum) {
        if (size == 0) throw std::invalid_argument("The size of Bloom filter cannot be zero");
        if ((size & (size - 1)) != 0) throw std::invalid_argument("The size of a Bloom filter must be a power of 2.");
        if (hashFunctionNum == 0) throw std::invalid_argument("The number of hash functions cannot be zero");

        // 初始化基本布隆过滤器
        this->bloomFilterSize = size;
        this->bloomFilter.resize(size, false);
        this->hashFunctionNum = hashFunctionNum;
        this->msgNum = 0;
    }

    ~BaseBloomFilter() { bloomFilter.clear(); }

    void add(const std::string& key) {
        std::vector<size_t> indices = calcHashIndices(key);
        for (const size_t index : indices) { bloomFilter[index % bloomFilterSize] = true; }
        ++msgNum;
    }

    bool contains(const std::string& key) const {
        std::vector<size_t> indices = calcHashIndices(key);
        for (const size_t index : indices) { if (!bloomFilter[index % bloomFilterSize]) { return false; } }
        return true;
    }

    unsigned long getMsgNum() const { return msgNum; }

private:
    std::vector<bool> bloomFilter;
    size_t bloomFilterSize = 0;
    unsigned int hashFunctionNum = 0;
    unsigned long msgNum = 0;

    // 通过哈希函数计算下标
    std::vector<size_t> calcHashIndices(const std::string& key) const {
        std::vector<size_t> indices(hashFunctionNum);
        for (unsigned int i = 0; i < hashFunctionNum; ++i) {
            const size_t hashResult = hashSHA256(key + "_" + std::to_string(i));
            indices[i] = hashResult;
        }
        return indices;
    }

    // 哈希函数SHA256
    static size_t hashSHA256(const std::string& key) {
        // SHA256散列算法输出32字节长度的哈希值
        SHA256::BYTE buf[SHA256_BLOCK_SIZE];
        SHA256::SHA256_CTX ctx;

        SHA256::sha256_init(&ctx);
        SHA256::sha256_update(&ctx, reinterpret_cast<const SHA256::BYTE*>(key.c_str()), key.length());
        SHA256::sha256_final(&ctx, buf);

        return reinterpret_cast<size_t>(buf);
    }
};

#endif //BASE_BLOOM_FILTER_HPP
