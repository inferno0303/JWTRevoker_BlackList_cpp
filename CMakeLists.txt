cmake_minimum_required(VERSION 3.28)
project(JWTRevoker_BlackList)

set(CMAKE_CXX_STANDARD 14)

add_executable(JWTRevoker_BlackList
        src/main.cpp
        src/core/BloomFilter/BloomFilter.h
        src/core/BloomFilter/BloomFilter.cpp
        src/core/SHA256/SHA256.h
        src/core/SHA256/SHA256.cpp
        src/core/NetworkConnection/ServerSocket.h
        src/core/NetworkConnection/ServerSocket.cpp
        src/core/BloomFilterEngine/BloomFilterEngine.cpp
        src/core/BloomFilterEngine/BloomFilterEngine.h
        src/core/BloomFilterScheduler/BloomFilterScheduler.cpp
        src/core/BloomFilterScheduler/BloomFilterScheduler.h
        src/core/Utils/nlohmann/json.hpp
)

# 链接 ws2_32 库
target_link_libraries(JWTRevoker_BlackList ws2_32)
