#ifndef TCP_SESSION_HPP
#define TCP_SESSION_HPP

#include <iostream>
#include <thread>
#include <atomic>
#include <boost/asio.hpp>
#include "../Utils/ThreadSafeQueue.hpp"
#include "../Utils/StringParser.hpp"
#include "../Utils/JsonStringMsg.hpp"

using boost::asio::io_context;
using boost::asio::awaitable;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;

inline std::string recvMsgFromSocket(tcp::socket &sock) {
    // 接收消息头
    char msgHeaderBE[4]{};
    boost::asio::read(sock, boost::asio::buffer(msgHeaderBE, 4));

    // 网络字节序转为主机字节序
    std::uint32_t msgBodyLength = 0;
    std::memcpy(&msgBodyLength, msgHeaderBE, 4);
    msgBodyLength = ntohl(msgBodyLength);

    if (msgBodyLength == 0) return {};
    std::vector<char> msgBody(msgBodyLength);
    boost::asio::read(sock, boost::asio::buffer(msgBody.data(), msgBodyLength));
    return {msgBody.begin(), msgBody.end()};
}

inline void sendMsgToSocket(tcp::socket &sock, const std::string &msg) {
    if (msg.empty()) return ;
    // 动态分配消息帧内存，包括 4 bytes 的消息长度和消息体
    std::vector<char> msgFrame(4 + msg.size());
    const auto msgLengthBE = htonl(msg.size());
    std::memcpy(msgFrame.data(), &msgLengthBE, 4);
    std::memcpy(msgFrame.data() + 4, msg.data(), msg.size());

    boost::asio::write(sock, boost::asio::buffer(msgFrame, 4 + msg.size()));
}

class MasterSession {
public:
    explicit MasterSession(const std::map<std::string, std::string> &config) : config_(config) {
        // 读取配置
        host_ = config.at("master_ip");
        port_ = stringToUShort(config_.at("master_port"));

        connect();
        watchDogRunFlag.store(true);
        sendRunFlag.store(true);
        recvRunFlag.store(true);
        watchDogThread = std::thread(&MasterSession::watchDog, this);
        sendThread = std::thread(&MasterSession::send, this);
        recvThread = std::thread(&MasterSession::recv, this);
    }

    ~MasterSession() {
        watchDogRunFlag.store(false);
        sendRunFlag.store(false);
        recvRunFlag.store(false);
        if (watchDogThread.joinable()) watchDogThread.join();
        if (sendThread.joinable()) sendThread.join();
        if (recvThread.joinable()) recvThread.join();
        sock.close();
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
    tcp::socket sock{io_context_};

    // 消息发送队列、消息接收队列
    ThreadSafeQueue<std::string> sendQueue{};
    ThreadSafeQueue<std::string> recvQueue{};

    // 消息发送线程
    std::thread sendThread;
    std::atomic<bool> sendRunFlag{false};

    // 消息接收线程
    std::thread recvThread;
    std::atomic<bool> recvRunFlag{false};

    // 看门狗线程
    bool connErrFlag = false;
    std::thread watchDogThread;
    std::atomic<bool> watchDogRunFlag{false};

    void watchDog() {
        while (watchDogRunFlag.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (connErrFlag) {
                std::cout << "Connection lost, try to reconnect." << std::endl;
                sendRunFlag.store(false);
                recvRunFlag.store(false);
                if (sendThread.joinable()) sendThread.join();
                if (recvThread.joinable()) recvThread.join();
                sock.close(); // 关闭socket
                connErrFlag = false; // 重置旗标
                connect(); // 重新连接
                sendRunFlag.store(true);
                recvRunFlag.store(true);
                sendThread = std::thread(&MasterSession::send, this);
                recvThread = std::thread(&MasterSession::recv, this);
            }
        }
    }

    void connect() {
        tcp::resolver resolver(io_context_);
        const auto endpoints = resolver.resolve(host_, std::to_string(port_));

        while (true) {
            boost::system::error_code ec;
            const auto connected_endpoint = boost::asio::connect(sock, endpoints, ec);
            if (!ec) {
                std::cout << "[Master connection] Master connected: " << connected_endpoint << std::endl;

                // 发送认证请求
                const std::string event = "hello_from_client";
                std::map<std::string, std::string> data;
                data["client_uid"] = config_.at("client_uid");
                data["token"] = config_.at("token");
                const std::string msg = msgAssembly(event, data);

                // 构建消息帧，包含4字节长度字段和消息体
                char msgFrame[4 + msg.size() + 1]{};
                auto msgLengthBE = htonl(msg.size());
                std::memcpy(msgFrame, &msgLengthBE, 4);
                std::memcpy(msgFrame + 4, msg.data(), msg.length());

                // 发送消息
                boost::asio::write(sock, boost::asio::buffer(msgFrame, 4 + msg.size()));

                // 接收认证结果
                char msgHeaderBE[4]{};
                boost::asio::read(sock, boost::asio::buffer(msgHeaderBE, 4));

                // 网络字节序转为主机字节序
                std::uint32_t msgBodyLength = 0;
                std::memcpy(&msgBodyLength, msgHeaderBE, 4);
                msgBodyLength = ntohl(msgBodyLength);

                // 根据消息体长度读消息体
                if (msgBodyLength == 0) throw std::runtime_error("Authenticate failed, incorrect response.");
                char msgBody[msgBodyLength + 1]{};
                boost::asio::read(sock, boost::asio::buffer(msgBody, msgBodyLength));

                // 解析消息体
                std::string event_;
                std::map<std::string, std::string> data_;
                msgParse(std::string(msgBody), event_, data_);

                if (event_ == "auth_success") {
                    std::cout << "[Master connection] Master Authenticate success" << std::endl;
                    break;
                }
                if (event_ == "auth_failed") throw std::runtime_error("Authenticate failed.");
            }
            std::cerr << "[Master connection] connection lost, try again after 5 sec: " << ec.message() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5)); // 等待5秒再尝试重新连接
        }
    }

    void send() {
        while (sendRunFlag.load()) {
            try {
                sendMsgToSocket(sock, sendQueue.dequeue());
            } catch (const std::exception &e) {
                connErrFlag = true;
                std::cerr << "Send Error: " << e.what() << std::endl;
                break;
            }
        }
    }

    void recv() {
        while (recvRunFlag.load()) {
            try {
                recvQueue.enqueue(recvMsgFromSocket(sock));
            } catch (const std::exception &e) {
                connErrFlag = true;
                std::cerr << "Receive Error: " << e.what() << std::endl;
                break;
            }
        }
    }
};


#endif // TCP_SESSION_HPP
