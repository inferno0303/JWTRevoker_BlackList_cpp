#include "BloomFilter.h"
#include "src/sha2/sha256/sha256.h"

BF::BloomFilter::BloomFilter(size_t size, unsigned int NUM_HASH_FUNCTION)
    : NUM_HASH_FUNCTION(NUM_HASH_FUNCTION)
{
    if (size == 0)
    {
        throw std::invalid_argument("Size of BloomFilter cannot be zero");
    }
    if (NUM_HASH_FUNCTION == 0)
    {
        throw std::invalid_argument("Number of hash functions cannot be zero");
    }
    bits.resize(size);
}

// 写入布隆过滤器
void BF::BloomFilter::add(const std::string &key)
{
    std::vector<size_t> hashIndices = getHashIndices(key);
    for (size_t index : hashIndices)
    {
        bits[index % bits.size()] = true;
    }
}

// 查询布隆过滤器
bool BF::BloomFilter::contains(const std::string &key) const
{
    std::vector<size_t> hashIndices = getHashIndices(key);
    for (size_t index : hashIndices)
    {
        if (!bits[index % bits.size()])
        {
            return false;
        }
    }
    return true;
}

// 通过哈希函数映射到布隆过滤器数组下标
std::vector<size_t> BF::BloomFilter::getHashIndices(const std::string &key) const
{
    std::vector<size_t> hashIndices;
    for (unsigned int i = 0; i < NUM_HASH_FUNCTION; ++i)
    {
        const size_t hash_result = hash_sha256(key + "_" + std::to_string(i));
        hashIndices.push_back(hash_result);
    }
    return hashIndices;
}

size_t BF::BloomFilter::hash_sha256(const std::string &key) const
{
    SHA256::BYTE buf[SHA256_BLOCK_SIZE];
    SHA256::SHA256_CTX ctx;

    SHA256::sha256_init(&ctx);
    SHA256::sha256_update(&ctx, (unsigned char *)key.c_str(), key.length());
    SHA256::sha256_final(&ctx, buf);

    return (size_t)buf;
}