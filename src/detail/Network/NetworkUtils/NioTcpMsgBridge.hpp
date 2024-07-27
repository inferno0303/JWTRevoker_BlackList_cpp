#ifndef NIO_TCP_MSG_BRIDGE_HPP
#define NIO_TCP_MSG_BRIDGE_HPP

#include <iostream>
#include <thread>
#include <atomic>
#include <cstring>
#include <winsock2.h>

#include "../../Utils/ThreadSafeQueue.hpp"

#define BUFFER_SIZE 1024
#define MSG_QUEUE_MAXSIZE 4096

class NioTcpMsgBridge {
public:
    explicit NioTcpMsgBridge(const SOCKET s, std::condition_variable& cv) : onSocketErrCv(cv) {
        if (s == INVALID_SOCKET) {
            throw std::runtime_error("NIOSocketSenderReceiver initialization failed: invalid socket.");
        }
        this->socket = s;

        // 启动发送线程
        sendThreadRunFlag.store(true);
        sendThread = std::thread(&NioTcpMsgBridge::sendMsgWorker, this);

        // 启动接收线程
        recvThreadRunFlag.store(true);
        recvThread = std::thread(&NioTcpMsgBridge::recvMsgWorker, this);
    }

    ~NioTcpMsgBridge() {
        // 在析构函数中停止所有线程
        sendThreadRunFlag.store(false);
        recvThreadRunFlag.store(false);
        if (sendThread.joinable()) sendThread.join();
        if (recvThread.joinable()) recvThread.join();
    }

    // 将消息放入发送消息队列（生产者）
    void asyncSendMsg(const std::string& msg) {
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

    // 停止工作
    void stop() {
        sendThreadRunFlag.store(false);
        recvThreadRunFlag.store(false);
        if (sendThread.joinable()) sendThread.join();
        if (recvThread.joinable()) recvThread.join();
    }

private:
    // 目标套接字
    SOCKET socket = INVALID_SOCKET;

    // 套接字错误时通知的条件变量
    std::condition_variable& onSocketErrCv;

    // 消息发送线程
    std::thread sendThread;
    std::atomic<bool> sendThreadRunFlag{false};

    // 消息发送队列
    ThreadSafeQueue<std::string> sendMsgQueue{MSG_QUEUE_MAXSIZE};

    // 消息接收线程
    std::thread recvThread;
    std::atomic<bool> recvThreadRunFlag{false};

    // 消息接收队列
    ThreadSafeQueue<std::string> recvMsgQueue{MSG_QUEUE_MAXSIZE};

    // 取出发送消息队列的消息（消费者），并写入到套接字发送缓冲区
    void sendMsgWorker() {
        while (sendThreadRunFlag.load()) {
            // 退队列头元素（如果队列为空，则阻塞，直到队列不为空）
            const std::string msg = sendMsgQueue.dequeue();

            // 1、构造消息头
            auto msgLengthBE = htonl(msg.length()); // 转换为大端序
            // 2、构造消息帧
            char msgFrame[4 + msg.length() + 1]{};
            // 3、复制消息头到消息帧的前4个字节
            std::memcpy(msgFrame, &msgLengthBE, 4);
            // 4、复制消息体到消息帧，跳过前4个字节
            std::memcpy(msgFrame + 4, msg.c_str(), msg.length());

            // 将消息帧写入到套接字的发送缓冲区
            size_t totalSent = 0;
            while (totalSent < 4 + msg.length()) {
                const int bytesSent = send(socket, msgFrame, static_cast<int>(4 + msg.length() - totalSent), 0);
                if (bytesSent == SOCKET_ERROR) {
                    std::cerr << "Send failed with error: " << WSAGetLastError() << std::endl;
                    onSocketErr();
                    return;
                }
                totalSent += bytesSent;
            }
        }
    }

    // 取出套接字缓冲区的内容，放入接收消息队列（生产者）
    void recvMsgWorker() {
        // 从套接字的接收缓冲区中获取信息
        while (recvThreadRunFlag.load()) {
            // 1、读数据头
            char msgHeaderBE[4]{};
            int totalReceived = 0;
            while (totalReceived < 4) {
                int bytesReceived = 0;
                bytesReceived = recv(socket, msgHeaderBE + totalReceived, 4 - totalReceived, 0);
                if (bytesReceived == SOCKET_ERROR) {
                    std::cerr << "Recv failed with error: " << WSAGetLastError() << std::endl;
                    onSocketErr();
                    return;
                }
                if (bytesReceived == 0) {
                    std::cerr << "Connection closed by the peer." << std::endl;
                    onSocketErr();
                    return;
                }
                totalReceived += bytesReceived;
            }

            // 2、将消息头转换为小端序
            unsigned int msgBodyLength = 0;
            std::memcpy(&msgBodyLength, msgHeaderBE, 4);
            msgBodyLength = ntohl(msgBodyLength);

            // 3、根据消息体长度读消息体
            char msgBody[msgBodyLength + 1]{};
            totalReceived = 0;
            while (totalReceived < msgBodyLength) {
                int bytesReceived = 0;
                bytesReceived = recv(socket, msgBody + totalReceived,
                                     static_cast<int>(msgBodyLength) - totalReceived, 0);
                if (bytesReceived == SOCKET_ERROR) {
                    std::cerr << "Recv failed with error: " << WSAGetLastError() << std::endl;
                    return;
                }
                if (bytesReceived == 0) {
                    std::cerr << "Connection closed by the peer." << std::endl;
                    return;
                }
                totalReceived += bytesReceived;
            }
            std::cout << "[received] " << msgBody << std::endl;
            recvMsgQueue.enqueue(std::string(msgBody));
        }
    }

    void onSocketErr() {
        if (sendThreadRunFlag.load() || recvThreadRunFlag.load()) {
            // 停止线程
            sendThreadRunFlag.store(false);
            recvThreadRunFlag.store(false);
            std::cout << "Socket error event.\n";

            // 通知外部
            onSocketErrCv.notify_all();
        }
    }
};

#endif // NIO_TCP_MSG_BRIDGE_HPP
