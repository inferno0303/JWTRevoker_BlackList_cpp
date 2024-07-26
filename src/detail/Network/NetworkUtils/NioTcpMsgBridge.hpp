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
    explicit NioTcpMsgBridge(const SOCKET s) {
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

        // 在析构函数中释放所有队列元素的内存
        while (!sendMsgQueue.empty()) {
            const char* str = sendMsgQueue.dequeue();
            delete[] str;
        }
        while (!recvMsgQueue.empty()) {
            const char* str = recvMsgQueue.dequeue();
            delete[] str;
        }
    }

    // 将消息放入发送消息队列（生产者）
    void sendMsg(const char* msg) {
        // 分配内存
        const auto newMsg = static_cast<char*>(std::malloc(std::strlen(msg)));
        if (newMsg) {
            // 将内存的内容设置为 '\0'
            std::memset(newMsg, '\0', std::strlen(msg));
            // 复制字符串
            std::strcpy(newMsg, msg);
            // 添加到队列
            sendMsgQueue.enqueue(newMsg);
        } else {
            throw std::runtime_error("Malloc memory faild.");
        }
    }

    // 取出接收消息队列的消息（消费者）
    const char* recvMsg() {
        // 退队列头元素（如果队列为空，则阻塞，直到队列不为空）
        const char* msg = recvMsgQueue.dequeue();
        // 返回
        return msg;
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
    // 目标套接字
    SOCKET socket = INVALID_SOCKET;

    // 消息发送线程
    std::thread sendThread;
    std::atomic<bool> sendThreadRunFlag{false};

    // 消息发送队列
    ThreadSafeQueue<const char*> sendMsgQueue{MSG_QUEUE_MAXSIZE};

    // 消息接收线程
    std::thread recvThread;
    std::atomic<bool> recvThreadRunFlag{false};

    // 消息接收队列
    ThreadSafeQueue<const char*> recvMsgQueue{MSG_QUEUE_MAXSIZE};

    // 取出发送消息队列的消息（消费者），并写入到套接字发送缓冲区
    void sendMsgWorker() {
        while (sendThreadRunFlag) {
            // 退队列头元素（如果队列为空，则阻塞，直到队列不为空）
            const char* const msg = sendMsgQueue.dequeue();

            // 1、创建消息帧
            const size_t msgFrameLength = 4 + strlen(msg);
            char msgFrame[msgFrameLength]{};
            // 2、构造消息头
            const size_t msgLength = strlen(msg);
            auto msgLengthBE = htonl(msgLength); // 转换为大端序
            // 3、将消息头复制到消息帧前4个字节
            std::memcpy(msgFrame, &msgLengthBE, 4);
            // 4、将消息复制到消息帧，从第5个字节开始
            std::memcpy(msgFrame + 4, msg, strlen(msg));

            // 清理
            delete[] msg;

            // 将待发送的信息写入到套接字的发送缓冲区中
            size_t sent = 0;
            while (sent < msgFrameLength) {
                const int result = send(socket, msgFrame, static_cast<int>(msgFrameLength - sent), 0);
                if (result == SOCKET_ERROR) {
                    std::cerr << "Send failed with error: " << WSAGetLastError() << std::endl;
                    return;
                }
                sent += result;
            }
        }
    }

    // 取出套接字缓冲区的内容，放入接收消息队列（生产者）
    void recvMsgWorker() {
        // 从套接字的接收缓冲区中获取信息
        while (recvThreadRunFlag) {
            // 1、读数据头
            char msgHeaderBE[4]{};
            int totalReceived = 0;
            while (totalReceived < 4) {
                const int bytesReceived = recv(socket, msgHeaderBE + totalReceived, 4 - totalReceived, 0);
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

            // 2、转换为小端序
            // const unsigned int msgBodyLength = ntohl(*reinterpret_cast<const unsigned int*>(msgHeaderBE)); // 这样做可能会导致字节对齐问题
            unsigned int msgBodyLength = 0;
            std::memcpy(&msgBodyLength, msgHeaderBE, 4);
            msgBodyLength = ntohl(msgBodyLength);


            // 3、根据消息体长度读消息体
            char msgBody[msgBodyLength + 1]{};
            totalReceived = 0;
            while (totalReceived < msgBodyLength) {
                const int bytesReceived = recv(socket, msgBody + totalReceived,
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

            // 分配内存
            const auto recvMsg = static_cast<char*>(std::malloc(msgBodyLength));
            if (recvMsg) {
                // 将内存的内容设置为 '\0'
                std::memset(recvMsg, '\0', msgBodyLength);
                // 复制字符串
                std::strcpy(recvMsg, msgBody);
                // 添加到队列
                recvMsgQueue.enqueue(recvMsg);
            } else {
                throw std::runtime_error("Malloc memory faild.");
            }
        }
    }
};

#endif // NIO_TCP_MSG_BRIDGE_HPP
