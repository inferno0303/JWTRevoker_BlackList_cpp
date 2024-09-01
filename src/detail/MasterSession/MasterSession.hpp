#ifndef TCP_SESSION_HPP
#define TCP_SESSION_HPP

#include <iostream>
#include <thread>
#include <atomic>
#include <string>
#include <boost/asio.hpp>
#include "../Utils/ThreadSafeQueue.hpp"
#include "../Utils/StringParser.hpp"
#include "../Utils/JsonSerializer.hpp"
#include "../Utils/SocketMsgFrame.hpp"

using boost::asio::io_context;
using boost::asio::awaitable;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;

class MasterSession {
public:
    explicit MasterSession(const std::map<std::string, std::string> &config) : config(config) {
        // 读取配置
        host = config.at("master_ip");
        port = stringToUShort(config.at("master_port"));

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
    const std::map<std::string, std::string> &config;
    std::string host;
    unsigned short port;

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
        const auto endpoints = resolver.resolve(host, std::to_string(port));

        while (true) {
            boost::system::error_code ec;
            const auto connected_endpoint = boost::asio::connect(sock, endpoints, ec);
            if (!ec) {
                std::cout << "[Master connection] Master connected: " << connected_endpoint << std::endl;

                // 发送认证请求
                const std::string event = "hello_from_client";
                std::map<std::string, std::string> data;
                data["client_uid"] = config.at("client_uid");
                data["token"] = config.at("token");
                const std::string msg = msgAssembly(event, data);
                sendMsgToSocket(sock, msg);

                // 接受认证请求
                std::string event_;
                std::map<std::string, std::string> data_;
                msgParse(recvMsgFromSocket(sock), event_, data_);

                // 判断是否认证成功
                if (event_ == "auth_success") {
                    std::cout << "[Master connection] Master Authenticate success" << std::endl;
                    break;
                }
                if (event_ == "auth_failed")
                    throw std::invalid_argument("Authenticate failed, node_uid: " + config.at("client_uid") +
                                                ", node_token: " + config.at("token"));
            }
            std::cerr << "[Master connection] Connection failure, try again after 5 sec: " << ec.message() << std::endl;
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
