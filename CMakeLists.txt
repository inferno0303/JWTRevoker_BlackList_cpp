cmake_minimum_required(VERSION 3.28)
project(JWTRevoker_BlackList)

set(CMAKE_CXX_STANDARD 11)

add_executable(JWTRevoker_BlackList
        src/main.cpp
        src/detail/BlackListEngine/SHA256/SHA256.h
        src/detail/BlackListEngine/SHA256/SHA256.cpp
        src/detail/BlackListEngine/BaseBloomFilter.hpp
        src/detail/BlackListEngine/Engine.hpp
        src/detail/ThirdPartyLibs/nlohmann/json.hpp
        src/detail/Utils/ThreadingUtils/ThreadSafeQueue.hpp
        src/detail/MasterConnector/MasterConnector.hpp
        src/detail/Utils/ConfigReader.hpp
        src/detail/Utils/NetworkUtils/MsgFormatter.hpp
        src/detail/Utils/StringParser.hpp
        src/detail/Server/Server.hpp
        src/detail/Utils/NetworkUtils/ConnectServer.hpp
        src/detail/Utils/NetworkUtils/TCPMsgHub.hpp
        src/detail/Scheduler/Scheduler.hpp
)

# 链接 ws2_32 库
target_link_libraries(JWTRevoker_BlackList ws2_32)
