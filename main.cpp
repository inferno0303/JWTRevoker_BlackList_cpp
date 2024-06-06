#include "BloomFilter.h"
#include <iostream>

int main() {
    try {
        // Initialize a Bloom Filter with 1000 bits
        BF::BloomFilter filter(1000);

        // Add some keys to the Bloom Filter
        filter.add("apple");
        filter.add("banana");
        filter.add("cherry");

        // Check for existence of keys
        std::cout << "Contains 'apple': " << filter.contains("apple") << std::endl;
        std::cout << "Contains 'banana': " << filter.contains("banana") << std::endl;
        std::cout << "Contains 'cherry': " << filter.contains("cherry") << std::endl;
        std::cout << "Contains 'date': " << filter.contains("date") << std::endl; // Expected output: 0 (false)
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}