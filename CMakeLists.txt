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
        src/detail/Utils/ThreadingUtils/ThreadSafeQueue.hpp
        src/detail/Utils/NetworkUtils/NioTcpMsgBridge.hpp
        src/detail/MasterServerConn/MasterServerConn.hpp
        src/detail/Utils/ConfigReader.hpp
        src/detail/BlackListManager/BlackListManager.hpp
        src/detail/Utils/NetworkUtils/MsgFormat.hpp
        src/detail/Utils/StringParser.hpp
        src/detail/TCPServer/Server.hpp
)

# 链接 ws2_32 库
target_link_libraries(JWTRevoker_BlackList ws2_32)
