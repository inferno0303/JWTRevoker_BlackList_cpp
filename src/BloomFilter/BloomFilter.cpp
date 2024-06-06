#include "BloomFilter.h"

namespace BF {

BloomFilter::BloomFilter(size_t size) {
    if (size == 0) {
        throw std::invalid_argument("Size of BloomFilter cannot be zero");
    }
    bits.resize(size);
}

void BloomFilter::add(const std::string &key) {
    auto hashIndices = getHashIndices(key);
    for (auto index : hashIndices) {
        bits[index % bits.size()] = true;
    }
}

bool BloomFilter::contains(const std::string &key) const {
    auto hashIndices = getHashIndices(key);
        for (auto index : hashIndices) {
            if (!bits[index % bits.size()]) {
                return false;
            }
        }
        return true;
}

std::vector<size_t> BloomFilter::getHashIndices(const std::string &key) const {
    return { hash1(key), hash2(key) };
}

size_t BloomFilter::hash1(const std::string &key) const {
    return std::hash<std::string>{}(key);
}

size_t BloomFilter::hash2(const std::string &key) const {
    size_t hash = 0;
    for (char c : key) {
        hash = hash * 31 + c;
    }
    return hash;
}

} // namespace BF