#ifndef TCPSERVER_SERVER_HPP
#define TCPSERVER_SERVER_HPP

#include <iostream>
#include <thread>
#include <atomic>
#include <map>

#include "../Utils/StringParser.hpp"
#include "../Utils/NetworkUtils/TCPMsgHub.hpp"
#include "../Utils/NetworkUtils/MsgFormatter.hpp"
#include "../BlackListEngine/Engine.hpp"

class Server {
public:
    explicit Server(const std::map<std::string, std::string> &_config, const Engine *_engine)
        : config(_config), engine(_engine) {
    }

    ~Server() {
        stop();
    }

    // 启动服务器监听
    void start() {
        const std::string &ip = config.at("server_ip");
        const unsigned short port = stringToUShort(config.at("server_port"));
        if (!serverThread.joinable()) {
            serverThreadRunFlag.store(true);
            serverThread = std::thread(&Server::listenWorker, this, ip, port);
        }
    }

    // 服务器监听事件循环
    void exec() {
        if (serverThread.joinable()) serverThread.join();
    }

    // 停止服务器监听
    void stop() {
        serverThreadRunFlag.store(false);
        if (serverThread.joinable()) {
            serverThread.join();
        }
    }

private:
    // 配置
    const std::map<std::string, std::string> config;

    // 引擎
    const Engine *engine;

    // 监听线程
    std::atomic<bool> serverThreadRunFlag{false};
    std::thread serverThread;

    void listenWorker(const std::string &ip, const unsigned short port) {
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
        if (bind(serverSocket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == SOCKET_ERROR) {
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
            if ((newSocket = accept(serverSocket, reinterpret_cast<sockaddr *>(&address), &addrlen)) ==
                INVALID_SOCKET) {
                const int errorCode = WSAGetLastError();
                closesocket(serverSocket);
                WSACleanup();
                throw std::runtime_error("Accept failed: " + std::to_string(errorCode));
            }

            std::cout << "New connection accepted." << std::endl;

            // 创建线程处理新的客户端连接
            std::thread(&Server::handleClientWorker, this, newSocket).detach();
        }
    }

    // 处理客户端线程
    void handleClientWorker(const SOCKET clientSock) const {
        bool runFlag = true;
        TCPMsgHub msgHub(clientSock, [&runFlag]() {
            runFlag = false;
        });

        // 接收数据线程，模拟处理数据较慢的情况
        // 需要添加停止逻辑
        std::thread handler([&msgHub, &runFlag, this] {
            while (runFlag) {
                std::string msg = msgHub.recvMsg();
                std::string event;
                std::map<std::string, std::string> data;
                doMsgParse(msg, event, data);

                // 查询是否撤回
                if (event == "query") {
                    const std::string token = data["token"];
                    const std::string expTime = data["exp_time"];
                    const bool result = engine->contain(token, stringToTimestamp(expTime));
                    if (result) {
                        data["result"] = "yes";
                    } else {
                        data["result"] = "no";
                    }
                    const std::string replyMsg = doMsgAssembly("query_result", data);
                    msgHub.asyncSendMsg(replyMsg);
                    continue;
                }

                std::cout << "Unknown msg event: " << event << std::endl;
            }
        });

        if (handler.joinable()) handler.join();
    }
};

#endif //TCPSERVER_SERVER_HPP
