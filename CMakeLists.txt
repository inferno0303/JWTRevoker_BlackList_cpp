cmake_minimum_required(VERSION 3.28)
project(JWTRevoker_BlackList)

set(CMAKE_CXX_STANDARD 11)

add_executable(JWTRevoker_BlackList
        src/main.cpp
        src/detail/BlackListEngine/SHA256/SHA256.h
        src/detail/BlackListEngine/SHA256/SHA256.cpp
        src/detail/BlackListEngine/BaseBloomFilter.hpp
        src/detail/BlackListEngine/BlackListEngine.hpp
        src/detail/ThirdPartyLibs/nlohmann/json.hpp
        src/detail/Utils/ThreadSafeQueue.hpp
        src/detail/Network/NetworkUtils/NioTcpMsgBridge.hpp
        src/detail/Network/MasterServerConn.hpp
        src/detail/Utils/ConfigReader.hpp
        src/detail/BlackListManager/BlackListManager.hpp
        src/detail/Network/NetworkUtils/MsgFormat.hpp
        src/detail/Utils/StringConverter.hpp
        src/detail/Network/ClientService.hpp
)

# 链接 ws2_32 库
target_link_libraries(JWTRevoker_BlackList ws2_32)
