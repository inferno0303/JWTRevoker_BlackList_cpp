// ServerSocket.h
#ifndef SERVER_SOCKET_H
#define SERVER_SOCKET_H

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <winsock2.h>
#include <vector>

#define BUFFER_SIZE 1024

namespace SERVER_SOCKET {

    class ServerSocket {
    private:
        // 初始化服务器套接字变量
        unsigned short PORT;
        WSADATA wsaData{};
        SOCKET serverSocket{}, clientSocket{};
        struct sockaddr_in serverAddr{}, clientAddr{};

        // 服务器监听线程
        std::atomic<bool> stopServerListenFlag{true};
        std::thread serverListenThread;
        void serverListenWorker();

        // 客户端处理线程
        void clientSocketWorker(SOCKET clientSocket) const;

    public:
        explicit ServerSocket(unsigned short port);
        ~ServerSocket();

        // 服务器监听线程
        void startServerListenThread();
        void stopServerListenThread();
    };
}

#endif // SERVER_SOCKET_H