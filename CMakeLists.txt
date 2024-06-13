cmake_minimum_required(VERSION 3.28)
project(JWTRevoker_BlackList)

set(CMAKE_CXX_STANDARD 14)

add_executable(JWTRevoker_BlackList
        src/main.cpp
        src/core/BloomFilter/BloomFilter.h
        src/core/BloomFilter/BloomFilter.cpp
        src/core/SHA256/SHA256.h
        src/core/SHA256/SHA256.cpp
        src/core/BloomFilterManager/BloomFilterManager.h
        src/core/BloomFilterManager/BloomFilterManager.cpp
        src/core/CycleRotationTimerTask/CycleRotationTimerTask.h
        src/core/CycleRotationTimerTask/CycleRotationTimerTask.cpp)
