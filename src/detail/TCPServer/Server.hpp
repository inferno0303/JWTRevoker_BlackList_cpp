#ifndef TCPSERVER_SERVER_HPP
#define TCPSERVER_SERVER_HPP

#include <iostream>
#include <thread>
#include <atomic>
#include <map>
#include <memory>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "../Utils/StringParser.hpp"
#include "../Utils/NetworkUtils/NioTcpMsgBridge.hpp"
#include "../Utils/NetworkUtils/MsgFormat.hpp"

#include "../BlackListEngine/BlackListEngine.hpp"

#pragma comment(lib, "ws2_32.lib")

class Server {
public:
    Server(std::map<std::string, std::string>& startupConfig, BlackListEngine& engine) :
        ip(startupConfig["client_service_ip"]),
        port(stringToUShort(startupConfig["client_service_ip"])),
        engine(engine) {
    }

    ~Server() {
        stop();
    }

    // 启动服务器监听
    void start() {
        if (!serverThread.joinable()) {
            serverThreadRunFlag.store(true);
            serverThread = std::thread(&Server::listenWorker, this, ip, port);
        }
    }

    // 停止服务器监听
    void stop() {
        serverThreadRunFlag.store(false);
        if (serverThread.joinable()) {
            serverThread.join();
        }
    }

    // 服务器监听事件循环
    void exec() {
        if (serverThread.joinable()) serverThread.join();
    }

private:
    // 监听的ip和端口
    const std::string& ip;
    const unsigned short port;

    // 黑名单引擎
    BlackListEngine& engine;

    // 监听线程
    std::atomic<bool> serverThreadRunFlag{false};
    std::thread serverThread;

    void listenWorker(const std::string& ip, const unsigned short& port) {
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
        if (inet_pton(AF_INET, ip.c_str(), &address.sin_addr) <= 0) {
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
            std::thread(handleClientWorker, this, newSocket).detach();
        }
    }

    // 处理客户端线程
    void handleClientWorker(const SOCKET clientSocket) const {
        // socket错误通知
        std::condition_variable onSocketErrCv;

        // 消息桥
        auto msgBridge = std::unique_ptr<NioTcpMsgBridge>(
            new NioTcpMsgBridge(clientSocket, onSocketErrCv)
        );

        // 接收数据线程，模拟处理数据较慢的情况
        // 需要添加停止逻辑
        std::thread processMsgThread([&msgBridge, this] {
            while (true) {
                std::string msg = msgBridge->recvMsg();
                std::string event;
                std::map<std::string, std::string> data;
                doMsgParse(msg, event, data);

                // 处理事件
                if (event == "query") {
                    const std::string token = data["token"];
                    const std::string expTime = data["exp_time"];
                    const bool result = engine.contain(token, stringToTimestamp(expTime));
                    if (result) {
                        data["result"] = "yes";
                    } else {
                        data["result"] = "no";
                    }
                    const std::string replyMsg = doMsgAssembly("query_result", data);
                    msgBridge->asyncSendMsg(replyMsg);
                    continue;
                }
                std::cout << "Unknow msg event: " << event << std::endl;
            }
        });
        if (processMsgThread.joinable()) processMsgThread.join();
        msgBridge->stop();
        msgBridge.reset();
    }
};

#endif //TCPSERVER_SERVER_HPP
