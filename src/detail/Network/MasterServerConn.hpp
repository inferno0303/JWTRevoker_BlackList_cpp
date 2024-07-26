#ifndef MASTER_SERVER_CONNECTOR_HPP
#define MASTER_SERVER_CONNECTOR_HPP

#include <iostream>
#include <thread>
#include <string>
#include <map>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "NetworkUtils/MsgFormat.hpp"
#include "NetworkUtils/NioTcpMsgBridge.hpp"
#include "../ThirdPartyLibs/nlohmann/json.hpp"


// 实现了与 master 服务器通信的逻辑
class MasterServerConn {
public:
    MasterServerConn(const char* server_ip, const unsigned short server_port) {
        // 连接 master 服务器
        this->server_socket = connectServer(server_ip, server_port);

        // 启动Tcp消息桥，消息桥（指针）将会共享给需要发送消息的线程
        this->nioTcpMsgBridge = new NioTcpMsgBridge(this->server_socket);

        // 发送认证消息
        this->sendAuthMsg();

        // 如果认证不成功
        if (!isAuthSuccess()) {
            throw std::runtime_error("Authenticate failed");
        }

        std::cout << "Success connect to master server: " << server_ip << ":" << server_port << std::endl;
    }

    ~MasterServerConn() {
        // 在析构函数中清理资源
        nioTcpMsgBridge = nullptr;
    }

    NioTcpMsgBridge* getNioTcpMsgBridge() const {
        return this->nioTcpMsgBridge;
    }

private:
    SOCKET server_socket = INVALID_SOCKET;
    NioTcpMsgBridge* nioTcpMsgBridge = nullptr;

    // 连接到服务器
    static SOCKET connectServer(const char* server_ip, const unsigned short server_port) {
        WSADATA wsaData{};
        auto sock = INVALID_SOCKET;
        sockaddr_in address = {};

        // 初始化WinSock
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::runtime_error("WSAStartup failed: " + std::to_string(WSAGetLastError()));
        }

        // 创建套接字
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
            const int errorCode = WSAGetLastError();
            WSACleanup();
            throw std::runtime_error("Socket creation error: " + std::to_string(errorCode));
        }

        // 设置地址和端口
        address.sin_family = AF_INET;
        if (inet_pton(AF_INET, server_ip, &address.sin_addr) <= 0) {
            closesocket(sock);
            WSACleanup();
            throw std::runtime_error("Invalid address/ Address not supported: " + std::string(server_ip));
        }
        address.sin_port = htons(server_port);

        // 连接到服务器
        if (connect(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
            const int errorCode = WSAGetLastError();
            closesocket(sock);
            WSACleanup();
            throw std::runtime_error("Connection Failed: " + std::to_string(errorCode));
        }

        std::cout << "Connected to server: " << server_ip << ":" << server_port << std::endl;

        return sock;
    }

    // 向服务器发送认证消息
    void sendAuthMsg() const {
        const std::string event = "hello_from_client";
        std::map<std::string, std::string> data;
        data["client_uid"] = "0001";
        data["token"] = "abcde";
        const std::string msg = doMsgAssembly(event, data);
        nioTcpMsgBridge->sendMsg(msg.c_str());
    }

    // 接收服务器的认证回复
    bool isAuthSuccess() const {
        const char* msg = nioTcpMsgBridge->recvMsg();
        std::string event;
        std::map<std::string, std::string> data;
        doMsgParse(std::string(msg), event, data);
        delete[] msg;
        if (event == "auth_success") return true;
        if (event == "auth_failed") return false;
        throw std::runtime_error("Cannot receive client authenticate reply");
    }
};

#endif // MASTER_SERVER_CONNECTOR_HPP
