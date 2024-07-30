#include <iostream>

#include "detail/Utils/ConfigReader.hpp"
#include "detail/Utils/NetworkUtils/NioTcpMsgBridge.hpp"
#include "detail/MasterServerConn/MasterServerConn.hpp"
#include "detail/BlackListEngine/BlackListEngine.hpp"
#include "detail/BlackListManager/BlackListManager.hpp"
#include "detail/TCPServer/Server.hpp"

#define CONFIG_FILE "C:\\MyProjects\\JWTRevoker_BlackList_cpp\\src\\config.txt"


int main() {
    std::cout << "JWTRevoker_BlackList is starting..." << std::endl;

    // 读取配置文件
    std::map<std::string, std::string> startupConfig = readConfig(CONFIG_FILE);

    // 连接到master服务器
    MasterServerConn masterServerConn(startupConfig);
    NioTcpMsgBridge* msgBridge = masterServerConn.getMsgBridge();

    // 初始化引擎
    BlackListEngine engine(startupConfig);

    // 初始化管理器
    const BlackListManager manager(engine, msgBridge, startupConfig);

    // 检查是否就绪
    if (!engine.isReady()) {
        return -1;
    }

    // 启动服务
    Server server(startupConfig, engine);
    server.exec();

    return 0;
}
