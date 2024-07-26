cmake_minimum_required(VERSION 3.28)
project(JWTRevoker_BlackList)

set(CMAKE_CXX_STANDARD 11)

add_executable(JWTRevoker_BlackList
        src/main.cpp
        src/detail/BloomFilterEngine/BloomFilter/BloomFilter.h
        src/detail/BloomFilterEngine/BloomFilter/BloomFilter.cpp
        src/detail/BloomFilterEngine/BloomFilter/SHA256/SHA256.h
        src/detail/BloomFilterEngine/BloomFilter/SHA256/SHA256.cpp
        src/detail/BloomFilterEngine/BloomFilterEngine.cpp
        src/detail/BloomFilterEngine/BloomFilterEngine.h
        src/detail/ThirdPartyLibs/nlohmann/json.hpp
        src/detail/Utils/ThreadSafeQueue.hpp
        src/detail/Network/NetworkUtils/NioTcpMsgBridge.hpp
        src/detail/Network/MasterServerConn.hpp
        src/detail/Utils/ConfigReader.hpp
        src/detail/BloomFilterManager/BloomFilterManager.hpp
        src/detail/Network/NetworkUtils/MsgFormat.hpp
        src/detail/Utils/StringConverter.hpp
        src/detail/Network/ClientService.hpp
)

# 链接 ws2_32 库
target_link_libraries(JWTRevoker_BlackList ws2_32)
