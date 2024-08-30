#include <iostream>

#include "detail/Utils/ConfigReader.hpp"
#include "detail/TcpSession/TcpSession.hpp"
#include "detail/Scheduler/Scheduler.hpp"
#include "detail/Engine/Engine.hpp"
// #include "detail/Server/Server.hpp"


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

    // 连接到master服务器

    TcpSession session(config);

    // 初始化调度器
    const Scheduler scheduler(config, session);

    // 初始化引擎
    const Engine* engine = scheduler.getEngine();

    // 启动服务
    // Server s(config, engine);
    // s.exec();

    std::cout << "Service is Running." << std::endl;

    std::getchar();

    return 0;
}
