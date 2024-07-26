#include <iostream>

#include "detail/Utils/ConfigReader.hpp"
#include "detail/Network/MasterServerConnector.hpp"
#include "detail/BloomFilterScheduler/BloomFilterManager.hpp"
// #include "detail/BloomFilterScheduler/BloomFilterScheduler.h"
// #include "detail/Network/ServerSocket.h"

#define PORT 8080

int main() {

    std::cout << "JWTRevoker_BlackList is starting..." << std::endl;

    // 读取配置文件
    const std::string filePath = R"(C:\Projects\JWTRevoker_BlackList_cpp\src\config.txt)";
    std::map<std::string, std::string> config = readConfig(filePath);
    const std::string master_server_ip = config["master_server_ip"];
    const std::string master_server_port = config["master_server_port"];
    const unsigned long master_server_port_ = std::stoul(master_server_port);
    if (master_server_port_ > std::numeric_limits<unsigned short>::max()) {
        throw std::out_of_range("Value exceeds range of unsigned short");
    }

    // 连接到控制服务器
    const MasterServerConnector masterServerConnector{master_server_ip.c_str(), static_cast<unsigned short>(master_server_port_)};

    // 连接成功后
    NioTcpMsgBridge* nioTcpMsgBridge = masterServerConnector.getNioTcpMsgBridge();
    BloomFilterManager bloom_filter_manager(nioTcpMsgBridge);





    //
    // BloomFilterScheduler bloomFilterScheduler{};
    //
    // // 启动对外服务
    // SERVER_SOCKET::ServerSocket serverSocket(PORT);
    // serverSocket.startServerListenThread();

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    // 停止任务

    return 0;
}

