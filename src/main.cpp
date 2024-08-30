#include <iostream>

#include "detail/Utils/ConfigReader.hpp"
#include "detail/Engine/Engine.hpp"
#include "detail/MasterSession/MasterSession.hpp"
#include "detail/Scheduler/Scheduler.hpp"
#include "detail/Server/TCPServer.hpp"


int main(const int argc, char* argv[]) {
    std::string configFilePath = R"(C:\MyProjects\JWTRevoker_BlackList_cpp\src\config.txt)"; // 默认配置文件路径

    // 解析命令行参数，查找 "-c" 参数来获取配置文件路径
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-c") {
            if (i + 1 < argc) { configFilePath = argv[i + 1]; }
            else {
                std::cerr << "Error: -c option requires a file path argument." << std::endl;
                return 1; // 返回错误代码
            }
            break;
        }
    }

    // 读取配置文件
    const std::map<std::string, std::string> config = readConfig(configFilePath);

    // 连接到 Master 服务器
    MasterSession session(config);

    // 启动引擎
    Engine engine(config);

    // 启动调度器
    const Scheduler scheduler(config, session, engine);

    // 启动对外服务
    TCPServer server(config, engine);
    server.run();

    return 0;
}
