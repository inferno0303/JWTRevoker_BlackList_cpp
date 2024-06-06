// BloomFilter.h
#ifndef BLOOM_FILTER_H
#define BLOOM_FILTER_H

#include <iostream>
#include <vector>
#include <string>
#include <functional>

namespace BF {

class BloomFilter {
public:
    explicit BloomFilter(size_t size);

    void add(const std::string &key);
    bool contains(const std::string &key) const;

private:
    std::vector<bool> bits;

    std::vector<size_t> getHashIndices(const std::string &key) const;
    size_t hash1(const std::string &key) const;
    size_t hash2(const std::string &key) const;
};

} // namespace BF

#endif // BLOOM_FILTER_H
