#include "BloomFilter.h"
#include "SHA256/SHA256.h"

BF::BloomFilter::BloomFilter(const size_t size, const unsigned int numHashFunction)
    : HASH_FUNCTION_NUM(numHashFunction) {
    if (size == 0) {
        throw std::invalid_argument("Size of BloomFilter cannot be zero");
    }
    if (HASH_FUNCTION_NUM == 0) {
        throw std::invalid_argument("Number of hash functions cannot be zero");
    }
    bits.resize(size);
}

void BF::BloomFilter::add(const std::string& key) {
    std::vector<size_t> hashIndices = getHashIndices(key);
    for (const size_t index : hashIndices) {
        bits[index % bits.size()] = true;
    }
    // 饱和度计数
    ++MSG_NUM;
}

bool BF::BloomFilter::contains(const std::string& key) const {
    std::vector<size_t> hashIndices = getHashIndices(key);
    for (const size_t index : hashIndices) {
        if (!bits[index % bits.size()]) {
            return false;
        }
    }
    return true;
}

std::vector<size_t> BF::BloomFilter::getHashIndices(const std::string& key) const {
    std::vector<size_t> hashIndices;
    for (unsigned int i = 0; i < HASH_FUNCTION_NUM; ++i) {
        const size_t hash_result = hash_sha256(key + "_" + std::to_string(i));
        hashIndices.push_back(hash_result);
    }
    return hashIndices;
}

size_t BF::BloomFilter::hash_sha256(const std::string& key) {
    SHA256::BYTE buf[SHA256_BLOCK_SIZE];
    SHA256::SHA256_CTX ctx;

    SHA256::sha256_init(&ctx);
    SHA256::sha256_update(&ctx, (unsigned char*)key.c_str(), key.length());
    SHA256::sha256_final(&ctx, buf);

    return reinterpret_cast<size_t>(buf);
}

unsigned long long BF::BloomFilter::getMSG_NUM() const {
    return MSG_NUM;
}
