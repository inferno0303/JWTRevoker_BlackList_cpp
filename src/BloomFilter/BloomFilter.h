// BloomFilter.h
#ifndef BLOOM_FILTER_H
#define BLOOM_FILTER_H

#include <iostream>
#include <vector>
#include <string>
#include <functional>

namespace BF
{

    class BloomFilter
    {
    private:
        // Bloom filter bits array
        std::vector<bool> bits;

        // The number of hash function
        unsigned int NUM_HASH_FUNCTION;

        std::vector<size_t> getHashIndices(const std::string &key) const;
        size_t hash_sha256(const std::string &key) const;

    public:
        BloomFilter(size_t size, unsigned int NUM_HASH_FUNCTION);
        
        // 写入布隆过滤器
        void add(const std::string &key);

        // 查询布隆过滤器
        bool contains(const std::string &key) const;
    };

} // namespace BF

#endif // BLOOM_FILTER_H
