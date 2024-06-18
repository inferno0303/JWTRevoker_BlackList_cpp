#include <iostream>
#include "core/BloomFilterManager/BloomFilterManager.h"
#include "core/CycleRotationTimerTask/CycleRotationTimerTask.h"
#include "core/NetworkConnection/ServerSocket.h"

#define PORT 8080

int main() {

    std::cout << "JWTRevoker_BlackList is Running!" << std::endl;

    // 初始化一些布隆过滤器
    BFM::BloomFilterManager bloomFilterManager(3600 * 24, 1, 8 * 1024, 5);

    // 创建线程，实现周期轮换定时任务
    CYCLE_ROTATION_TIMER_TASK::CycleRotationTimerTask cycleRotationTimerTask(bloomFilterManager);
    cycleRotationTimerTask.start();

    // 创建线程，实现服务器监听
    SERVER_SOCKET::ServerSocket serverSocket(PORT);
    serverSocket.startServerListenThread();

    // 由于线程是不会自己停止的，这里我们可以模拟等待信号或处理逻辑
    // 比如捕捉中断信号等
    std::cin.get(); // 等待用户输入来继续，可以用作简单的停止信号

    // 停止任务
    cycleRotationTimerTask.stop();

    return 0;
}