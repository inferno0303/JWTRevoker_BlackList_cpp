#include <iostream>
#include "core/BloomFilters/BloomFilters.h"
#include "core/CycleRotationTimerTask/CycleRotationTimerTask.h"
#include "core/NetworkConnection/ServerSocket.h"

#define PORT 8080

int main() {

    std::cout << "JWTRevoker_BlackList is Running!" << std::endl;

    // 初始化一些布隆过滤器
    const BFS::BloomFilters bloom_filters(3600 * 24, 1, 8 * 1024, 5);

    // 创建线程，实现周期轮换定时任务
    CYCLE_ROTATION_TIMER_TASK::CycleRotationTimerTask cycleRotationTimerTask(bloom_filters);
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

// #include <iostream>
// #include <cstring>
// #include <winsock2.h>
// #include <ws2tcpip.h>
// #include <chrono>
// #include <thread>
//
// #pragma comment(lib, "ws2_32.lib")
//
// #define SERVER_IP "127.0.0.1"
// #define SERVER_PORT 9999
// #define BUFFER_SIZE 1024
// #define INTERVAL 5 // 时间间隔，单位：秒
//
// int main() {
//     WSADATA wsaData;
//     SOCKET sock = INVALID_SOCKET;
//     struct sockaddr_in serv_addr;
//     char buffer[BUFFER_SIZE] = {0};
//     const char *hello = "hello from client";
//
//     // 初始化Winsock
//     if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
//         std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl;
//         return -1;
//     }
//
//     // 创建套接字
//     if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
//         std::cerr << "Socket creation error: " << WSAGetLastError() << std::endl;
//         WSACleanup();
//         return -1;
//     }
//
//     serv_addr.sin_family = AF_INET;
//     serv_addr.sin_port = htons(SERVER_PORT);
//
//     // 将IPv4地址转换成二进制形式
//     if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
//         std::cerr << "Invalid address/ Address not supported" << std::endl;
//         closesocket(sock);
//         WSACleanup();
//         return -1;
//     }
//
//     // 连接到服务端
//     if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
//         std::cerr << "Connection Failed: " << WSAGetLastError() << std::endl;
//         closesocket(sock);
//         WSACleanup();
//         return -1;
//     }
//
//     send(sock, hello, strlen(hello), 0);
//     std::cout << "Message sent: " << hello << std::endl;
//
//     // 定时发送消息
//     while (true) {
//         // 接收服务端的响应（可选）
//         int valread = recv(sock, buffer, BUFFER_SIZE, 0);
//         if (valread > 0) {
//             buffer[valread] = '\0';
//             std::cout << "Server response: " << buffer << std::endl;
//         }
//
//         // 等待指定时间间隔
//         // std::this_thread::sleep_for(std::chrono::seconds(INTERVAL));
//     }
//
//     // 关闭套接字
//     closesocket(sock);
//     WSACleanup();
//
//     return 0;
}
