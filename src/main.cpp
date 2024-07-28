#include <iostream>

#include "detail/Utils/ConfigReader.hpp"
#include "detail/Utils/StringConverter.hpp"
#include "detail/Network/MasterServerConn.hpp"
#include "detail/BlackListManager/BlackListManager.hpp"
#include "detail/Network/ClientService.hpp"


int main() {
    std::cout << "JWTRevoker_BlackList is starting..." << std::endl;

    // 读取配置文件
    std::map<std::string, std::string> config = readConfig(CONFIG_FILE_PATH);

    // 读取启动参数
    const std::string master_server_ip = config["master_server_ip"];
    const std::string master_server_port = config["master_server_port"];
    const std::string keepalive_interval = config["keepalive_interval"];
    const std::string client_service_ip = config["client_service_ip"];
    const std::string client_service_port = config["client_service_port"];

    // 连接到 master 服务器
    const MasterServerConn masterServerConn(master_server_ip, stringToUShort(master_server_port),
                                            stringToUInt(keepalive_interval));

    // 初始化布隆过滤器
    BlackListManager manager(masterServerConn.getMsgBridge(), config);

    auto engine = manager.getEngine();

    // 监听客户端
    ClientService clientService(client_service_ip.c_str(), stringToUShort(client_service_port), engine);

    // 服务器事件循环
    clientService.exec();

    return 0;
}
