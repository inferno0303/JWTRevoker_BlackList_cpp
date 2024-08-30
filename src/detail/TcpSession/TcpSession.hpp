#ifndef TCP_SESSION_HPP
#define TCP_SESSION_HPP

#include <iostream>
#include <thread>
#include <atomic>
#include <boost/asio.hpp>
#include "../Utils/ThreadSafeQueue.hpp"
#include "../Utils/StringParser.hpp"
#include "../Utils/MsgFormatter.hpp"

using boost::asio::io_context;
using boost::asio::ip::tcp;

class TcpSession {
public:
    explicit TcpSession(const std::map<std::string, std::string> &config) : config_(config) {
        // 读取配置
        host_ = config.at("master_ip");
        port_ = stringToUShort(config_.at("master_port"));

        connect();
        watchDogThreadRunFlag.store(true);
        sendThreadRunFlag.store(true);
        recvThreadRunFlag.store(true);
        watchDogThread = std::thread(&TcpSession::watchDogThread, this);
        sendThread = std::thread(&TcpSession::sendWorker, this);
        recvThread = std::thread(&TcpSession::recvWorker, this);
    }

    ~TcpSession() {
        watchDogThreadRunFlag.store(false);
        sendThreadRunFlag.store(false);
        recvThreadRunFlag.store(false);
        if (watchDogThread.joinable()) watchDogThread.join();
        if (sendThread.joinable()) sendThread.join();
        if (recvThread.joinable()) recvThread.join();
        socket_.close();
    }

    // 将消息放入发送消息队列（生产者）
    void asyncSendMsg(const std::string &msg) { sendQueue.enqueue(msg); }

    // 取出接收消息队列的消息（消费者）
    std::string recvMsg() { return recvQueue.dequeue(); }

private:
    const std::map<std::string, std::string> &config_;
    std::string host_;
    unsigned short port_;

    io_context io_context_;
    tcp::socket socket_{io_context_};

    // 消息发送队列、消息接收队列
    ThreadSafeQueue<std::string> sendQueue{};
    ThreadSafeQueue<std::string> recvQueue{};

    // 消息发送线程
    std::thread sendThread;
    std::atomic<bool> sendThreadRunFlag{false};

    // 消息接收线程
    std::thread recvThread;
    std::atomic<bool> recvThreadRunFlag{false};

    // 看门狗线程
    bool connErrFlag = false;
    std::thread watchDogThread;
    std::atomic<bool> watchDogThreadRunFlag{false};

    void watchDogWorker() {
        while (watchDogThreadRunFlag.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            // 如果发生了错误，则停止发送和接收
            if (connErrFlag) {
                sendThreadRunFlag.store(false);
                recvThreadRunFlag.store(false);
                if (sendThread.joinable()) sendThread.join();
                if (recvThread.joinable()) recvThread.join();
                socket_.close();
            }
            connErrFlag = false; // 重置旗标
            connect();
            sendThreadRunFlag.store(true);
            recvThreadRunFlag.store(true);
            sendThread = std::thread(&TcpSession::sendWorker, this);
            recvThread = std::thread(&TcpSession::recvWorker, this);
        }
    }


    void connect() {
        tcp::resolver resolver(io_context_);
        const auto endpoints = resolver.resolve(host_, std::to_string(port_));

        while (true) {
            boost::system::error_code ec;
            const auto connected_endpoint = boost::asio::connect(socket_, endpoints, ec);

            if (!ec) {
                std::cout << "Connected to " << connected_endpoint << std::endl;

                // 发送认证请求
                const std::string event = "hello_from_client";
                std::map<std::string, std::string> data;
                data["client_uid"] = config_.at("client_uid");
                data["token"] = config_.at("token");
                const std::string msg = doMsgAssembly(event, data);

                // 构建消息帧，包含4字节长度字段和消息体
                char msgFrame[4 + msg.size() + 1]{};
                auto msgLengthBE = htonl(msg.size());
                std::memcpy(msgFrame, &msgLengthBE, 4);
                std::memcpy(msgFrame + 4, msg.data(), msg.length());

                // 发送消息
                boost::asio::write(socket_, boost::asio::buffer(msgFrame, 4 + msg.size()));

                // 接收认证结果
                char msgHeaderBE[4]{};
                boost::asio::read(socket_, boost::asio::buffer(msgHeaderBE, 4));

                // 网络字节序转为主机字节序
                std::uint32_t msgBodyLength = 0;
                std::memcpy(&msgBodyLength, msgHeaderBE, 4);
                msgBodyLength = ntohl(msgBodyLength);

                // 根据消息体长度读消息体
                if (msgBodyLength == 0) throw std::runtime_error("Authenticate failed, incorrect response.");
                char msgBody[msgBodyLength + 1]{};
                boost::asio::read(socket_, boost::asio::buffer(msgBody, msgBodyLength));

                // 解析消息体
                std::string event_;
                std::map<std::string, std::string> data_;
                doMsgParse(std::string(msgBody), event_, data_);

                if (event_ == "auth_success") break;
                if (event_ == "auth_failed") throw std::runtime_error("Authenticate failed.");
            }
            std::cerr << "Failed to connect: " << ec.message() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5)); // 等待5秒再尝试重新连接
        }
    }

    void sendWorker() {
        while (sendThreadRunFlag.load()) {
            try {
                const std::string msg = sendQueue.dequeue();
                if (msg.empty()) { continue; }

                // 构建消息帧，包含4字节长度字段和消息体
                char msgFrame[4 + msg.size() + 1]{};
                auto msgLengthBE = htonl(msg.size());
                std::memcpy(msgFrame, &msgLengthBE, 4);
                std::memcpy(msgFrame + 4, msg.data(), msg.length());

                boost::asio::write(socket_, boost::asio::buffer(msgFrame, 4 + msg.size()));

                std::cout << "[Sent] " << msg << std::endl;
            } catch (const std::exception &e) {
                std::cerr << "Send Error: " << e.what() << std::endl;
                connErrFlag = true;
                break;
            }
        }
    }

    void recvWorker() {
        while (recvThreadRunFlag.load()) {
            try {
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
            } catch (const std::exception &e) {
                std::cerr << "Receive Error: " << e.what() << std::endl;
                connErrFlag = true;
                break;
            }
        }
    }
};


#endif // TCP_SESSION_HPP
