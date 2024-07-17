// BloomFilter.h
#ifndef BLOOM_FILTER_H
#define BLOOM_FILTER_H

#include <iostream>
#include <vector>

namespace BF {

    class BloomFilter {
    private:
        // 布隆过滤器数组
        std::vector<bool> bits;

        // 哈希函数的数量
        unsigned int NUM_HASH_FUNCTION;

        // 通过哈希函数计算数组下标
        std::vector<size_t> getHashIndices(const std::string &key) const;

        // 哈希函数（由SHA256实现）
        static size_t hash_sha256(const std::string &key) ;

        // 布隆过滤器饱和度计数
        unsigned long long NUM_MSG{0};

    public:
        // 构造函数
        BloomFilter(size_t size, unsigned int numHashFunction);

        // 写入布隆过滤器
        void add(const std::string &key);

        // 查询布隆过滤器
        bool contains(const std::string &key) const;

        // 获取布隆过滤器饱和度计数
        unsigned long long getNUM_MSG() const;
    };

} // namespace BF

#endif // BLOOM_FILTER_H
