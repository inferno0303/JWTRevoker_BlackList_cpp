#ifndef MASTER_SERVER_CONNECTOR_HPP
#define MASTER_SERVER_CONNECTOR_HPP

#include <iostream>
#include <thread>
#include <string>
#include <map>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <condition_variable>

#include "../Utils/StringParser.hpp"
#include "../Utils/NetworkUtils/MsgFormat.hpp"
#include "../Utils/NetworkUtils/NioTcpMsgBridge.hpp"
#include "../ThirdPartyLibs/nlohmann/json.hpp"


// 实现了与 master 服务器通信的逻辑
class MasterServerConn {
public:
    explicit MasterServerConn(std::map<std::string, std::string>& startupConfig) :
        ip(startupConfig["master_server_ip"]),
        port(stringToUShort(startupConfig["master_server_port"])),
        keepaliveInterval(stringToUInt(startupConfig["keepalive_interval"])) {

        // 连接master服务器
        const auto sock = connectServer(ip, port);
        // 消息桥
        auto* msgBridge = new NioTcpMsgBridge(sock, onMsgBridgeErrCv);
        msgBridge->start();
        // 认证
        if (!doAuth(msgBridge)) {
            throw std::runtime_error("Authenticate failed!");
        }
        // 认证成功
        this->msgBridge = msgBridge;
        std::cout << "Success connect to master server: " << ip << ":" << port << std::endl;

        // 启动重新连接线程
        if (!reconnectThread.joinable()) {
            reconnectThreadRunFlag.store(true);
            reconnectThread = std::thread(&MasterServerConn::reconnectWorker, this);
        }

        // 启动发送心跳包线程
        if (!keepaliveThread.joinable()) {
            keepaliveThreadRunFlag.store(true);
            keepaliveThread = std::thread(&MasterServerConn::keepaliveWorker, this);
        }
    }

    ~MasterServerConn() {
        // 停止重新连接线程
        reconnectThreadRunFlag.store(false);
        if (reconnectThread.joinable()) {
            reconnectThread.join();
        }

        // 停止发送心跳包线程
        keepaliveThreadRunFlag.store(false);
        if (keepaliveThread.joinable()) {
            keepaliveThread.join();
        }
    }

    NioTcpMsgBridge* getMsgBridge() {
        return msgBridge;
    }

private:
    // 配置信息
    const std::string& ip;
    const unsigned short& port;
    const unsigned int keepaliveInterval;

    // 连接成功后的套接字
    SOCKET serverSocket = INVALID_SOCKET;
    // 消息桥、消息桥错误通知
    NioTcpMsgBridge* msgBridge = nullptr;
    std::condition_variable onMsgBridgeErrCv;

    static SOCKET connectServer(const std::string& server_ip, const unsigned short& server_port) {
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
        if (inet_pton(AF_INET, server_ip.c_str(), &address.sin_addr) <= 0) {
            closesocket(sock);
            WSACleanup();
            throw std::runtime_error("Invalid address/ Address not supported: " + std::string(server_ip));
        }
        address.sin_port = htons(server_port);

        // 连接到服务器
        while (true) {
            try {
                if (connect(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
                    const int errorCode = WSAGetLastError();
                    closesocket(sock);
                    WSACleanup();
                    throw std::runtime_error("Connection Failed: " + std::to_string(errorCode));
                }
                std::cout << "Successfully connected to the server: " << server_ip << ":" << server_port << std::endl;
                break;
            } catch (std::exception& e) {
                std::cout << "Failed to connect the server, try again after 5 seconds... " << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }
        return sock;
    }

    // 重新连接线程
    std::atomic<bool> reconnectThreadRunFlag{false};
    std::thread reconnectThread;

    void reconnectWorker() {
        while (reconnectThreadRunFlag) {
            std::mutex mtx;
            std::unique_lock<std::mutex> lock(mtx);
            onMsgBridgeErrCv.wait(lock);
            std::cout << "Try to reconnect master server..." << std::endl;

            auto sock = connectServer(ip, port);
            msgBridge->setSocket(sock);
            msgBridge->start();
            if (!doAuth(msgBridge)) {
                throw std::runtime_error("Authenticate failed!");
            }
            std::cout << "Reconnect to master server: " << ip << ":" << port << std::endl;
        }
    }


    // 向服务器验证身份
    bool doAuth(NioTcpMsgBridge* msgBridge) const {
        // 发送认证信息
        const std::string event = "hello_from_client";
        std::map<std::string, std::string> data;
        data["client_uid"] = "0001";
        data["token"] = "abcde";
        const std::string msg = doMsgAssembly(event, data);
        msgBridge->asyncSendMsg(msg);
        // 接收认证信息
        const std::string newMsg = msgBridge->recvMsg();
        std::string recvEvent;
        std::map<std::string, std::string> recvData;
        doMsgParse(newMsg, recvEvent, recvData);
        if (recvEvent == "auth_success") return true; // 认证成功
        if (recvEvent == "auth_failed") return false; // 认证失败
        throw std::runtime_error("Cannot receive client authenticate reply"); // 无法识别的消息类型
    }

    // 发送心跳包线程
    std::atomic<bool> keepaliveThreadRunFlag{false};
    std::thread keepaliveThread;

    void keepaliveWorker() const {
        while (keepaliveThreadRunFlag) {
            std::this_thread::sleep_for(std::chrono::seconds(keepaliveInterval));

            const std::string event = "keepalive";
            std::map<std::string, std::string> data;
            data["client_uid"] = "0001";
            const std::string msg = doMsgAssembly(event, data);
            msgBridge->asyncSendMsg(msg);
        }
    }
};

#endif // MASTER_SERVER_CONNECTOR_HPP
