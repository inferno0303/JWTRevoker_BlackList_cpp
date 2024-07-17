#include <iostream>
#include "core/BloomFilterScheduler/BloomFilterScheduler.h"
#include "core/NetworkConnection/ServerSocket.h"

#define PORT 8080

int main() {

    std::cout << "JWTRevoker_BlackList is starting..." << std::endl;

    BloomFilterScheduler bloomFilterScheduler{};
    // std::shared_ptr<BloomFilterEngine> bloomFilterEngine = bloomFilterScheduler.getBloomFilterEngine();

    // 创建线程，实现服务器监听
    SERVER_SOCKET::ServerSocket serverSocket(PORT);
    serverSocket.startServerListenThread();

    // 由于线程是不会自己停止的，这里我们可以模拟等待信号或处理逻辑
    // 比如捕捉中断信号等
    std::cin.get(); // 等待用户输入来继续，可以用作简单的停止信号

    // 停止任务

    return 0;
}

