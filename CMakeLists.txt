cmake_minimum_required(VERSION 3.28)
project(JWTRevoker_BlackList)

set(CMAKE_CXX_STANDARD 20)

include_directories("include")

add_executable(JWTRevoker_BlackList
        src/main.cpp
        src/detail/Engine/SHA256/SHA256.h
        src/detail/Engine/SHA256/SHA256.cpp
        src/detail/Engine/BaseBloomFilter.hpp
        src/detail/MasterConnector/MasterConnector.hpp
        src/detail/Engine/Engine.hpp
        src/detail/Scheduler/Scheduler.hpp
        src/detail/Utils/StringParser.hpp
        src/detail/Utils/ThreadingUtils/ThreadSafeQueue.hpp
        src/detail/Utils/ConfigReader.hpp
        src/detail/Utils/NetworkUtils/MsgFormatter.hpp
        src/detail/Utils/NetworkUtils/ConnectServer.hpp
        src/detail/Utils/NetworkUtils/TCPMsgHub.hpp
        src/detail/Server/Server.hpp
        src/detail/MasterConnector/TcpClient.hpp
)


if (WIN32)
    # 链接 ws2_32 mswsock 库
    target_link_libraries(JWTRevoker_BlackList ws2_32 mswsock)
endif ()
