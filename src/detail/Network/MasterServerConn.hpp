#ifndef MASTER_SERVER_CONNECTOR_HPP
#define MASTER_SERVER_CONNECTOR_HPP

#include <iostream>
#include <thread>
#include <string>
#include <map>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <condition_variable>

#include "NetworkUtils/MsgFormat.hpp"
#include "NetworkUtils/NioTcpMsgBridge.hpp"
#include "../ThirdPartyLibs/nlohmann/json.hpp"

#define MSG_QUEUE_MAXSIZE 4096


// 实现了与 master 服务器通信的逻辑
class MasterServerConn {
public:
    MasterServerConn(const std::string& ip, const unsigned short& port,
                     const unsigned int keepaliveInterval) : ip(ip), port(port), keepaliveInterval(keepaliveInterval) {
        // 连接 master 服务器
        tryToConnectMaster();

        // 认证
        if (!tryAuth()) {
            throw std::runtime_error("Authenticate failed");
        }

        std::cout << "Success connect to master server: " << ip << ":" << port << std::endl;

        // 启动重新连接线程
        // if (!reconnectThread.joinable()) {
        //     reconnectThreadRunFlag.store(true);
        //     reconnectThread = std::thread(&MasterServerConn::reconnectWorker, this);
        // }

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

    NioTcpMsgBridge& getMsgBridge() const {
        return *this->msgBridge;
    }

private:
    const std::string& ip;
    const unsigned short& port;
    SOCKET serverSocket = INVALID_SOCKET;

    // 消息发送队列
    ThreadSafeQueue<std::string> sendMsgQueue{MSG_QUEUE_MAXSIZE};

    // 消息接收队列
    ThreadSafeQueue<std::string> recvMsgQueue{MSG_QUEUE_MAXSIZE};

    // 消息桥
    std::unique_ptr<NioTcpMsgBridge> msgBridge = nullptr;

    // socket错误通知
    std::condition_variable onSocketErrCv;

    unsigned int keepaliveInterval = 0;

    // 连接到 master 并验证身份
    void tryToConnectMaster() {
        while (true) {
            try {
                this->serverSocket = connectServer(this->ip, this->port);
                break;
            } catch (std::exception& e) {
                std::cout << "Faild to connect master server, trying to reconnect after 5 seconds. " << e.what() <<
                    std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }
        if (msgBridge != nullptr) {
            msgBridge->stop();
            msgBridge.reset();
        }
        msgBridge = std::unique_ptr<NioTcpMsgBridge>(
            new NioTcpMsgBridge(serverSocket, sendMsgQueue, recvMsgQueue, onSocketErrCv)
        );
    }

    // 重新连接线程
    std::atomic<bool> reconnectThreadRunFlag{false};
    std::thread reconnectThread;

    void reconnectWorker() {
        while (reconnectThreadRunFlag) {
            std::mutex mtx;
            std::unique_lock<std::mutex> lock(mtx);
            onSocketErrCv.wait(lock);

            std::cout << "Reconnect to master server..." << std::endl;
            tryToConnectMaster();
            if (!tryAuth()) {
                throw std::runtime_error("Authenticate failed");
            }
            std::cout << "Reconnect to master server: " << ip << ":" << port << std::endl;
        }
    }

    // 连接到服务器
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
        if (connect(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
            const int errorCode = WSAGetLastError();
            closesocket(sock);
            WSACleanup();
            throw std::runtime_error("Connection Failed: " + std::to_string(errorCode));
        }

        std::cout << "Connected to server: " << server_ip << ":" << server_port << std::endl;

        return sock;
    }

    // 向服务器验证身份
    bool tryAuth() const {
        const std::string event = "hello_from_client";
        std::map<std::string, std::string> data;
        data["client_uid"] = "0001";
        data["token"] = "abcde";
        const std::string msg = doMsgAssembly(event, data);
        msgBridge->asyncSendMsg(msg);

        const std::string msg_ = msgBridge->recvMsg();
        std::string event_;
        std::map<std::string, std::string> data_;
        doMsgParse(msg_, event_, data_);
        if (event_ == "auth_success") return true;
        if (event_ == "auth_failed") return false;
        throw std::runtime_error("Cannot receive client authenticate reply");
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
