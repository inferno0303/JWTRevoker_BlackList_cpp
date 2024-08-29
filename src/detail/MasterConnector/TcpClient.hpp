#ifndef TCP_CLIENT_HPP
#define TCP_CLIENT_HPP

#include <iostream>
#include <thread>
#include <atomic>

#include <boost/asio.hpp>

#include "../Utils/ThreadingUtils/ThreadSafeQueue.hpp"

#define BUFFER_SIZE 1024
#define MSG_QUEUE_MAXSIZE 40960

using boost::asio::io_context;
using boost::asio::ip::tcp;

class TcpClient {
public:
    TcpClient(const std::string &host, const unsigned short port)
        : host_(host), port_(port), io_context_(), socket_(io_context_) {
        connect();

        sendThreadRunFlag.store(true);
        recvThreadRunFlag.store(true);

        sendThread = std::thread(&TcpClient::sendWorker, this);
        recvThread = std::thread(&TcpClient::recvWorker, this);
    }

    ~TcpClient() {
        sendThreadRunFlag.store(false);
        recvThreadRunFlag.store(false);
        if (sendThread.joinable()) sendThread.join();
        if (recvThread.joinable()) recvThread.join();
        socket_.close();
    }

    // 将消息放入发送消息队列（生产者）
    void asyncSendMsg(const std::string &msg) { sendQueue.enqueue(msg); }

    // 取出接收消息队列的消息（消费者）
    std::string recvMsg() { return recvQueue.dequeue(); }

private:
    const std::string &host_;
    const unsigned short port_;
    io_context io_context_;
    tcp::socket socket_;

    // 消息发送队列、消息接收队列
    ThreadSafeQueue<std::string> sendQueue{};
    ThreadSafeQueue<std::string> recvQueue{};

    // 消息发送线程
    std::thread sendThread;
    std::atomic<bool> sendThreadRunFlag{false};

    // 消息接收线程
    std::thread recvThread;
    std::atomic<bool> recvThreadRunFlag{false};

    void connect() {
        tcp::resolver resolver(io_context_);
        const auto endpoints = resolver.resolve(host_, std::to_string(port_));

        while (true) {
            boost::system::error_code ec;
            const auto connected_endpoint = boost::asio::connect(socket_, endpoints, ec);

            if (!ec) {
                std::cout << "Connected to " << connected_endpoint << std::endl;
                break;
            }
            std::cerr << "Failed to connect: " << ec.message() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5)); // 等待5秒再尝试重新连接
        }
    }

    void sendWorker() {
        while (sendThreadRunFlag.load()) {
            const std::string msg = sendQueue.dequeue();
            if (msg.empty()) { continue; }

            // 构建消息帧，包含4字节长度字段和消息体
            char msgFrame[4 + msg.size() + 1]{};
            auto msgLengthBE = htonl(msg.size());
            std::memcpy(msgFrame, &msgLengthBE, 4);
            std::memcpy(msgFrame + 4, msg.data(), msg.length());

            boost::asio::write(socket_, boost::asio::buffer(msgFrame, 4 + msg.size()));

            std::cout << "[Sent] " << msg << std::endl;
        }
    }

    void recvWorker() {
        while (recvThreadRunFlag.load()) {
            // 接收消息头
            char msgHeaderBE[4]{};
            boost::asio::read(socket_, boost::asio::buffer(msgHeaderBE, 4));

            // 网络字节序转为主机字节序
            std::uint32_t msgBodyLength = 0;
            std::memcpy(&msgBodyLength, msgHeaderBE, 4);
            msgBodyLength = ntohl(msgBodyLength);

            // 根据消息体长度读消息体
            if (msgBodyLength == 0) continue; // 防止接收空字符串
            char msgBody[msgBodyLength + 1]{};
            boost::asio::read(socket_, boost::asio::buffer(msgBody, msgBodyLength));

            recvQueue.enqueue(std::string(msgBody));

            std::cout << "[Received] " << msgBody << std::endl;
        }
    }
};


#endif //TCP_CLIENT_HPP
