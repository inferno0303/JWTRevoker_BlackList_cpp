#ifndef CONNECT_SERVER_HPP
#define CONNECT_SERVER_HPP

#include <iostream>
#include <string>
#include <stdexcept>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>

// For Windows
inline SOCKET connectServer(const std::string& ip, const unsigned short port) {
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
    if (inet_pton(AF_INET, ip.c_str(), &address.sin_addr) <= 0) {
        closesocket(sock);
        WSACleanup();
        throw std::runtime_error("Invalid address/ Address not supported: " + ip);
    }
    address.sin_port = htons(port);

    // 连接到服务器
    while (true) {
        try {
            if (connect(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
                const int errorCode = WSAGetLastError();
                throw std::runtime_error("Connection Failed: " + std::to_string(errorCode));
            }
            break;
        }
        catch (std::exception& e) {
            std::cout << "Failed to connect the server, try again after 5 seconds... " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    return sock;
}

#endif // CONNECT_SERVER_HPP
