#ifndef STRINGCONVERTER_HPP
#define STRINGCONVERTER_HPP

#include <iostream>
#include <string>
#include <vector>

inline unsigned short stringToUShort(const std::string& str) {
    try {
        return std::stoul(str);
    } catch (const std::invalid_argument& e) {
        std::cerr << "Invalid argument: " << e.what() << std::endl;
        return 0; // or handle error
    } catch (const std::out_of_range& e) {
        std::cerr << "Out of range: " << e.what() << std::endl;
        return 0; // or handle error
    }
}

inline unsigned int stringToUInt(const std::string& str) {
    try {
        return std::stoul(str);
    } catch (const std::invalid_argument& e) {
        std::cerr << "Invalid argument: " << e.what() << std::endl;
        return 0; // or handle error
    } catch (const std::out_of_range& e) {
        std::cerr << "Out of range: " << e.what() << std::endl;
        return 0; // or handle error
    }
}

inline size_t stringToSizeT(const std::string& str) {
    try {
        return std::stoull(str);
    } catch (const std::invalid_argument& e) {
        std::cerr << "Invalid argument: " << e.what() << std::endl;
        return 0; // or handle error
    } catch (const std::out_of_range& e) {
        std::cerr << "Out of range: " << e.what() << std::endl;
        return 0; // or handle error
    }
}

inline std::string vectorToString(const std::vector<unsigned long long>& vec) {
    std::string result = "[";

    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) {
            result += ",";
        }
        result += std::to_string(vec[i]);
    }

    result += "]";
    return result;
}

#endif //STRINGCONVERTER_HPP
