#include <iostream>
#include "detail/Utils/ConfigReader.hpp"
#include "detail/Network/MasterServerConnector/MasterServerHandler.hpp"
// #include "detail/BloomFilterScheduler/BloomFilterScheduler.h"
// #include "detail/Network/ServerSocket.h"

#define PORT 8080

int main() {

    std::cout << "JWTRevoker_BlackList is starting..." << std::endl;

    // 读取配置文件
    const std::string filePath = R"(C:\Projects\JWTRevoker_BlackList_cpp\src\config.txt)";
    std::unordered_map<std::string, std::string> config = readConfig(filePath);
    const std::string master_server_ip = config["master_server_ip"];
    const std::string master_server_port = config["master_server_port"];

    // 使用 std::stoul 转换为 unsigned long
    const unsigned long temp = std::stoul(master_server_port);

    // 检查转换后的值是否适合 unsigned short
    if (temp > std::numeric_limits<unsigned short>::max()) {
        throw std::out_of_range("Value exceeds range of unsigned short");
    }

    // 连接到控制服务器
    const MasterServerHandler masterServerHandler{master_server_ip.c_str(), static_cast<unsigned short>(temp)};
    NioTcpMsgSenderReceiver* n = masterServerHandler.getNioTcpMsgSenderReceiver();



    //
    // BloomFilterScheduler bloomFilterScheduler{};
    //
    // // 启动对外服务
    // SERVER_SOCKET::ServerSocket serverSocket(PORT);
    // serverSocket.startServerListenThread();
    //
    // // 由于线程是不会自己停止的，这里我们可以模拟等待信号或处理逻辑
    // // 比如捕捉中断信号等
    // std::cin.get(); // 等待用户输入来继续，可以用作简单的停止信号

    // 停止任务

    return 0;
}

