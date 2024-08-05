#ifndef TCP_MSG_HUB_HPP
#define TCP_MSG_HUB_HPP

#include <iostream>
#include <thread>
#include <atomic>
#include <cstring>
#include <future>
#include <winsock2.h>

#include "ConnectServer.hpp"
#include "../ThreadingUtils/ThreadSafeQueue.hpp"

inline void setColor(const int color) {
    // Get the console handle
    const auto hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    // Set the text color
    SetConsoleTextAttribute(hConsole, color);
}

#define BUFFER_SIZE 1024
#define MSG_QUEUE_MAXSIZE 40960

class TCPMsgHub {
public:
    // 作为客户端，主动连接服务器，并在断开连接时主动重连
    explicit TCPMsgHub(const std::string &ip, const unsigned short port) {
        // 连接到服务器，启动发送和接收线程
        startConnection(ip, port);

        // 启动连接看门狗线程
        watchDogThreadRunFlag.store(true);
        watchDogThread = std::thread(&TCPMsgHub::watchDogAsAClient, this, ip, port, nullptr);
    }

    // 作为客户端，主动连接服务器，并在断开连接时主动重连，在重连后异步执行回调函数
    explicit TCPMsgHub(const std::string &ip, const unsigned short port,
                       const std::function<void()> &watchDogCallback) {
        // 连接到服务器，启动发送和接收线程
        startConnection(ip, port);

        // 启动连接看门狗线程
        watchDogThreadRunFlag.store(true);
        watchDogThread = std::thread(&TCPMsgHub::watchDogAsAClient, this, ip, port, watchDogCallback);
    }

    // 作为服务端，监听客户端套接字，并在断开连接时触发回调函数，通知外部清理资源
    explicit TCPMsgHub(const SOCKET sock, const std::function<void()> &watchDogCallback) {
        // 监听目标套接字，启动发送和接收线程
        startListen(sock);

        // 启动连接看门狗线程
        watchDogThreadRunFlag.store(true);
        watchDogThread = std::thread(&TCPMsgHub::watchDogAsAServer, this, watchDogCallback);
    }

    ~TCPMsgHub() {
        // 停止看门狗线程
        watchDogThreadRunFlag.store(false);
        if (watchDogThread.joinable()) watchDogThread.join();

        // 关闭连接
        closeConnection();
    }

    // 将消息放入发送消息队列（生产者）
    void asyncSendMsg(const std::string &msg) {
        // 将字符串复制一份放到队列中
        sendMsgQueue.enqueue(msg);
    }

    // 取出接收消息队列的消息（消费者）
    std::string recvMsg() {
        return recvMsgQueue.dequeue();
    }

    // 发送消息队列长度
    size_t sendMsgQueueSize() const {
        return sendMsgQueue.size();
    }

    // 接收消息队列长度
    size_t recvMsgQueueSize() const {
        return recvMsgQueue.size();
    }

private:
    SOCKET sock = INVALID_SOCKET;

    void startConnection(const std::string &ip, const unsigned short port) {
        sock = connectServer(ip, port);
        sendThreadRunFlag.store(true);
        recvThreadRunFlag.store(true);
        sendThread = std::thread(&TCPMsgHub::sendMsgWorker, this, sock);
        recvThread = std::thread(&TCPMsgHub::recvMsgWorker, this, sock);
    }

    void startListen(const SOCKET sock) {
        sendThreadRunFlag.store(true);
        recvThreadRunFlag.store(true);
        sendThread = std::thread(&TCPMsgHub::sendMsgWorker, this, sock);
        recvThread = std::thread(&TCPMsgHub::recvMsgWorker, this, sock);
    }

    void closeConnection() {
        // 停止发送和接收线程
        sendThreadRunFlag.store(false);
        recvThreadRunFlag.store(false);
        if (sendThread.joinable()) sendThread.join();
        if (recvThread.joinable()) recvThread.join();
        closesocket(sock);
    }

    // 看门狗线程
    bool connectionErrFlag = false;
    std::thread watchDogThread;
    std::atomic<bool> watchDogThreadRunFlag{false};

    // 作为客户端，主动连接服务器，并在断开连接时主动重连
    void watchDogAsAClient(const std::string &ip, const unsigned short port, const std::function<void()> &callback) {
        while (watchDogThreadRunFlag.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (connectionErrFlag) {
                closeConnection();
                connectionErrFlag = false;
                startConnection(ip, port);
                if (callback) {
                    std::future<void> result = std::async(std::launch::async, callback);
                }
            }
        }
    }

    // 作为服务端，监听客户端套接字，并在断开连接时触发回调函数，通知外部清理资源
    void watchDogAsAServer(const std::function<void()> &callback) {
        while (watchDogThreadRunFlag.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (connectionErrFlag) {
                closeConnection();
                if (callback) {
                    std::future<void> result = std::async(std::launch::async, callback);
                }
                break;
            }
        }
    }

    // 消息发送队列、消息接收队列
    ThreadSafeQueue<std::string> sendMsgQueue{MSG_QUEUE_MAXSIZE};
    ThreadSafeQueue<std::string> recvMsgQueue{MSG_QUEUE_MAXSIZE};

    // 消息发送线程
    std::thread sendThread;
    std::atomic<bool> sendThreadRunFlag{false};

    // 消息接收线程
    std::thread recvThread;
    std::atomic<bool> recvThreadRunFlag{false};

    // 取出发送消息队列的消息（消费者），并写入到套接字发送缓冲区
    void sendMsgWorker(const SOCKET sock) {
        while (sendThreadRunFlag.load()) {
            // 退队列头元素（如果队列为空，则阻塞，直到队列不为空）
            const std::string msg = sendMsgQueue.dequeue();

            // 防止发送空字符串
            if (msg.empty()) continue;

            // 1、构造消息头
            auto msgLengthBE = htonl(msg.length()); // 转换为大端序
            // 2、构造消息帧
            char msgFrame[4 + msg.length() + 1]{};
            // 3、复制消息头到消息帧的前4个字节
            std::memcpy(msgFrame, &msgLengthBE, 4);
            // 4、复制消息体到消息帧，跳过前4个字节
            std::memcpy(msgFrame + 4, msg.c_str(), msg.length());

            // 5、将消息帧写入到套接字的发送缓冲区
            size_t totalSent = 0;
            while (totalSent < 4 + msg.length()) {
                const int bytesSent = send(sock, msgFrame, static_cast<int>(4 + msg.length() - totalSent), 0);
                if (bytesSent == SOCKET_ERROR) {
                    std::cerr << "Send failed with error: " << WSAGetLastError() << std::endl;
                    connectionErrFlag = true;
                    return;
                }
                totalSent += bytesSent;
            }
            // std::cout << "[Sent] " << msg << std::endl;
        }
    }

    // 取出套接字缓冲区的内容，放入接收消息队列（生产者）
    void recvMsgWorker(const SOCKET sock) {
        while (recvThreadRunFlag.load()) {
            // 1、接收消息头
            char msgHeaderBE[4]{};
            int totalReceived = 0;
            while (totalReceived < 4) {
                int bytesReceived = 0;
                bytesReceived = recv(sock, msgHeaderBE + totalReceived, 4 - totalReceived, 0);
                if (bytesReceived == SOCKET_ERROR) {
                    std::cerr << "Recv failed with error: " << WSAGetLastError() << std::endl;
                    connectionErrFlag = true;
                    return;
                }
                if (bytesReceived == 0) {
                    std::cerr << "Connection closed by the peer." << std::endl;
                    connectionErrFlag = true;
                    return;
                }
                totalReceived += bytesReceived;
            }

            // 2、将消息头转换为小端序
            unsigned int msgBodyLength = 0;
            std::memcpy(&msgBodyLength, msgHeaderBE, 4);
            msgBodyLength = ntohl(msgBodyLength);

            // 3、根据消息体长度读消息体
            if (msgBodyLength == 0) continue; // 防止接收空字符串
            char msgBody[msgBodyLength + 1]{};
            totalReceived = 0;
            while (totalReceived < msgBodyLength) {
                const int bytesReceived = recv(sock, msgBody + totalReceived,
                                               static_cast<int>(msgBodyLength) - totalReceived, 0);
                if (bytesReceived == SOCKET_ERROR) {
                    std::cerr << "Recv failed with error: " << WSAGetLastError() << std::endl;
                    connectionErrFlag = true;
                    return;
                }
                if (bytesReceived == 0) {
                    std::cerr << "Connection closed by the peer." << std::endl;
                    connectionErrFlag = true;
                    return;
                }
                totalReceived += bytesReceived;
            }
            setColor(2);
            std::cout << "[Received] " << msgBody << std::endl;
            setColor(7);
            recvMsgQueue.enqueue(std::string(msgBody));
        }
    }
};

#endif //TCP_MSG_HUB_HPP
