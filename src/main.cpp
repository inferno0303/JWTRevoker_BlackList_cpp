#include <iostream>

#include "detail/Utils/ConfigReader.hpp"
#include "detail/Utils/StringConverter.hpp"
#include "detail/Network/MasterServerConn.hpp"
#include "detail/BlackListManager/BlackListManager.hpp"
#include "detail/Network/ClientService.hpp"

#define PORT 8080

int main() {
    std::cout << "JWTRevoker_BlackList is starting..." << std::endl;

    // 读取配置文件
    const std::string filePath = R"(C:\Projects\JWTRevoker_BlackList_cpp\src\config.txt)";
    std::map<std::string, std::string> startupConfig = readConfig(filePath);

    // 读取启动参数
    const std::string master_server_ip = startupConfig["master_server_ip"];
    const std::string master_server_port = startupConfig["master_server_port"];
    const std::string keepalive_interval = startupConfig["keepalive_interval"];
    const std::string client_service_ip = startupConfig["client_service_ip"];
    const std::string client_service_port = startupConfig["client_service_port"];

    // 连接到 master 服务器
    const MasterServerConn masterServerConn(master_server_ip, stringToUShort(master_server_port),
                                            stringToUInt(keepalive_interval));

    // 初始化布隆过滤器
    BloomFilterManager bloomFilterManager(masterServerConn.getMsgBridge(), startupConfig);

    // 监听客户端
    ClientService clientService(client_service_ip.c_str(), stringToUShort(client_service_port));

    // 服务器事件循环
    clientService.exec();

    return 0;
}
