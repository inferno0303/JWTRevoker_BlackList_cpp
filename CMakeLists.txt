cmake_minimum_required(VERSION 3.28)
project(JWTRevoker_BlackList)

set(CMAKE_CXX_STANDARD 11)

add_executable(JWTRevoker_BlackList
        src/main.cpp
        src/detail/BloomFilter/BloomFilter.h
        src/detail/BloomFilter/BloomFilter.cpp
        src/detail/BloomFilter/SHA256/SHA256.h
        src/detail/BloomFilter/SHA256/SHA256.cpp
        src/detail/Network/ServerSocket.h
        src/detail/Network/ServerSocket.cpp
        src/detail/BloomFilterEngine/BloomFilterEngine.cpp
        src/detail/BloomFilterEngine/BloomFilterEngine.h
        src/detail/BloomFilterScheduler/BloomFilterScheduler.cpp
        src/detail/BloomFilterScheduler/BloomFilterScheduler.h
        src/detail/ThirdPartyLibs/nlohmann/json.hpp
        src/detail/Utils/ThreadSafeQueue.hpp
        src/detail/Network/NetworkUtils/NioTcpMsgSenderReceiver.hpp
        src/detail/Network/MasterServerConnector/MasterServerHandler.hpp
        src/detail/Utils/ConfigReader.hpp
        src/detail/BloomFilterScheduler/BloomFilterManager.hpp
)

# 链接 ws2_32 库
target_link_libraries(JWTRevoker_BlackList ws2_32)
