#ifndef MASTER_SERVER_HANDLER_HPP
#define MASTER_SERVER_HANDLER_HPP

#include <iostream>
#include <thread>
#include <sstream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <random>
#include <string>
#include "../NetworkUtils/NioTcpMsgSenderReceiver.hpp"
#include "../../ThirdPartyLibs/nlohmann/json.hpp"


// 封装了与 master 服务器通信的实现细节
class MasterServerHandler {
public:
    MasterServerHandler(const char* server_ip, const unsigned short server_port) {
        this->socket = connectToServer(server_ip, server_port);
        this->nioTcpMsgSenderReceiver = new NioTcpMsgSenderReceiver(this->socket);
        sendAuthMsg();
        if (!isAuthSuccess()) {
            throw std::runtime_error("Authenticate failed");
        }
    }

    ~MasterServerHandler() {
        // 在析构函数中清理资源
        nioTcpMsgSenderReceiver = nullptr;
    }

    NioTcpMsgSenderReceiver* getNioTcpMsgSenderReceiver() const {
        return this->nioTcpMsgSenderReceiver;
    }

private:
    auto socket = INVALID_SOCKET;
    NioTcpMsgSenderReceiver* nioTcpMsgSenderReceiver = nullptr;

    // 连接到服务器
    static SOCKET connectToServer(const char* server_ip, const unsigned short server_port) {
        WSADATA wsaData{};
        auto clientSocket = INVALID_SOCKET;
        sockaddr_in address = {};

        // 初始化WinSock
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::runtime_error("WSAStartup failed: " + std::to_string(WSAGetLastError()));
        }

        // 创建套接字
        if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
            const int errorCode = WSAGetLastError();
            WSACleanup();
            throw std::runtime_error("Socket creation error: " + std::to_string(errorCode));
        }

        // 设置地址和端口
        address.sin_family = AF_INET;
        if (inet_pton(AF_INET, server_ip, &address.sin_addr) <= 0) {
            closesocket(clientSocket);
            WSACleanup();
            throw std::runtime_error("Invalid address/ Address not supported: " + std::string(server_ip));
        }
        address.sin_port = htons(server_port);

        // 连接到服务器
        if (connect(clientSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
            const int errorCode = WSAGetLastError();
            closesocket(clientSocket);
            WSACleanup();
            throw std::runtime_error("Connection Failed: " + std::to_string(errorCode));
        }

        std::cout << "Connected to server: " << server_ip << ":" << server_port << std::endl;

        return clientSocket;
    }

    // 向服务器发送认证消息
    void sendAuthMsg() const {
        nlohmann::json data;
        data["client_uid"] = "0001";
        data["token"] = "abcde";
        const std::string msg = R"({"event":"hello_from_client","data":)" + data.dump() + "}";
        nioTcpMsgSenderReceiver->sendMsg(msg.c_str());
    }

    // 接收服务器的认证回复
    bool isAuthSuccess() const {
        const char* msg = nioTcpMsgSenderReceiver->recvMsg();
        nlohmann::json jsonObject = nlohmann::json::parse(msg);
        const std::string event = jsonObject["event"];
        if (event == "auth_success") return true;
        if (event == "auth_failed") return false;
        throw std::runtime_error("Cannot receive client authenticate reply");
    }
};

#endif // MASTER_SERVER_HANDLER_HPP
