#ifndef CLIENTSERVICE_HPP
#define CLIENTSERVICE_HPP

#include <iostream>
#include <thread>
#include <atomic>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "NetworkUtils/NioTcpMsgBridge.hpp"

#pragma comment(lib, "ws2_32.lib")

class ClientService {
public:
    ClientService(const char* ip, const unsigned short port) {
        // 启动服务器监听线程
        if (!serverThread.joinable()) {
            serverThreadRunFlag.store(true);
            serverThread = std::thread(&ClientService::serverWorker, "127.0.0.1", 9998);
        }
    }

    ~ClientService() {
        // 停止服务器监听线程
        serverThreadRunFlag.store(false);
        if (serverThread.joinable()) {
            serverThread.join();
        }
    }

    // 服务器事件循环
    void exec() {
        if (serverThread.joinable()) serverThread.join();
    }

private:
    // 监听线程
    std::atomic<bool> serverThreadRunFlag{false};
    std::thread serverThread;

    static void serverWorker(const char* ip, const unsigned short port) {
        WSADATA wsaData{};
        auto serverSocket = INVALID_SOCKET;
        sockaddr_in address = {};

        // 初始化WinSock
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::runtime_error("WSAStartup failed: " + std::to_string(WSAGetLastError()));
        }

        // 创建 serverSocket
        if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
            const int errorCode = WSAGetLastError();
            WSACleanup();
            throw std::runtime_error("Socket creation error: " + std::to_string(errorCode));
        }

        // 设置地址和端口
        address.sin_family = AF_INET;
        if (inet_pton(AF_INET, ip, &address.sin_addr) <= 0) {
            closesocket(serverSocket);
            WSACleanup();
            throw std::runtime_error("Invalid address/ Address not supported: " + std::string(ip));
        }
        address.sin_port = htons(port);

        // 绑定套接字到端口
        if (bind(serverSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
            const int errorCode = WSAGetLastError();
            closesocket(serverSocket);
            WSACleanup();
            throw std::runtime_error("Bind failed: " + std::to_string(errorCode));
        }

        // 监听端口
        if (listen(serverSocket, 3) == SOCKET_ERROR) {
            const int errorCode = WSAGetLastError();
            closesocket(serverSocket);
            WSACleanup();
            throw std::runtime_error("Listen failed: " + std::to_string(errorCode));
        }

        std::cout << "Server listening on port " << port << "..." << std::endl;

        while (true) {
            auto newSocket = INVALID_SOCKET;
            int addrlen = sizeof(address);
            if ((newSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&address), &addrlen)) == INVALID_SOCKET) {
                const int errorCode = WSAGetLastError();
                closesocket(serverSocket);
                WSACleanup();
                throw std::runtime_error("Accept failed: " + std::to_string(errorCode));
            }

            std::cout << "New connection accepted." << std::endl;

            // 创建线程处理新的客户端连接
            std::thread(handleClientWorker, newSocket).detach();
        }
    }

    // 处理客户端线程
    static void handleClientWorker(const SOCKET clientSocket) {
        // 创建 NIO 对象
        NioTcpMsgBridge nioTcpMsgBridge(clientSocket);

        // 接收数据线程，模拟处理数据较慢的情况
        // 需要添加停止逻辑
        std::thread processMsgThread([&nioTcpMsgBridge] {
            while (true) {
                std::string msg = nioTcpMsgBridge.recvMsg();
                std::cout << "[process] " << msg << " recvMsgQueue size: " << nioTcpMsgBridge.recvMsgQueueSize() << std::endl;
            }
        });
        if (processMsgThread.joinable()) processMsgThread.join();
    }
};

#endif //CLIENTSERVICE_HPP
