#ifndef MASTER_CONNECTOR_HPP
#define MASTER_CONNECTOR_HPP

#include <thread>
#include <string>
#include <map>

#include "../Utils/StringParser.hpp"
#include "../Utils/NetworkUtils/TCPMsgHub.hpp"
#include "../Utils/NetworkUtils/MsgFormatter.hpp"
#include "../Utils/ThreadingUtils/ThreadSafeQueue.hpp"


// 实现了与master服务器通信的逻辑
class MasterConnector {
public:
    explicit MasterConnector(const std::map<std::string, std::string> &_config)
        : config(_config) {
        // 启动消息桥
        const std::string &ip = config.at("master_ip");
        const unsigned short port = stringToUShort(config.at("master_port"));
        const std::function<void()> reconnCallback = std::bind(&MasterConnector::auth, this); // 用于重新发送认证请求
        msgHub = new TCPMsgHub(ip, port, reconnCallback);

        // 启动接收消息线程
        if (!receiveMsgThread.joinable()) {
            receiveMsgRunFlag.store(true);
            receiveMsgThread = std::thread(&MasterConnector::receiveMsgWorker, this);
        }

        // 身份认证
        auth();

        // 启动发送心跳包线程
        if (!keepaliveThread.joinable()) {
            keepaliveThreadRunFlag.store(true);
            keepaliveThread = std::thread(&MasterConnector::keepaliveWorker, this,
                                          stringToUInt(config.at("keepalive_interval")));
        }

        std::cout << "Successful connect to master server: " << ip << ":" << port << std::endl;
    }

    ~MasterConnector() {
        // 停止发送心跳包线程
        keepaliveThreadRunFlag.store(false);
        if (keepaliveThread.joinable()) {
            keepaliveThread.join();
        }

        free(msgHub);
    }

    std::string recvMsg() {
        return receivedMsgQueue.dequeue();
    }

    void asyncSendMsg(const std::string &msg) const {
        if (msgHub) {
            msgHub->asyncSendMsg(msg);
        }
    }

private:
    // 配置
    const std::map<std::string, std::string> &config;

    // 消息桥
    TCPMsgHub *msgHub;

    // 接收消息线程
    std::string authResult; // 认证结果
    std::mutex mtx; // 用于通知认证结果
    std::condition_variable cv; // 用于通知认证结果
    ThreadSafeQueue<std::string> receivedMsgQueue; // 其他消息类型放到这个队列里
    std::atomic<bool> receiveMsgRunFlag{false};
    std::thread receiveMsgThread;

    void receiveMsgWorker() {
        while (receiveMsgRunFlag) {
            if (!msgHub) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            std::string msg = msgHub->recvMsg();
            std::string event;
            std::map<std::string, std::string> data;
            doMsgParse(msg, event, data);

            // 只处理认证消息
            if (event == "auth_success" || event == "auth_failed") {
                std::lock_guard<std::mutex> lock(mtx);
                authResult = event;
                cv.notify_one();
                continue;
            }

            // 其他消息类型，则交给其他线程处理
            receivedMsgQueue.enqueue(msg);
        }
    }

    // 认证请求
    void auth() {
        // 发送认证请求
        const std::string event = "hello_from_client";
        std::map<std::string, std::string> data;
        data["client_uid"] = "0001";
        data["token"] = "abcde";
        const std::string msg = doMsgAssembly(event, data);
        if (!msgHub) return;
        msgHub->asyncSendMsg(msg);

        // 等待认证结果
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return authResult == "auth_success" || authResult == "auth_failed"; });
        if (authResult == "auth_success") {
            return;
        }
        if (authResult == "auth_failed") {
            throw std::runtime_error("Authenticate failed!"); // 认证失败
        }
    }

    // 发送心跳包线程
    std::atomic<bool> keepaliveThreadRunFlag{false};
    std::thread keepaliveThread;

    void keepaliveWorker(const unsigned int interval) const {
        while (keepaliveThreadRunFlag) {
            std::this_thread::sleep_for(std::chrono::seconds(interval));
            const std::string event = "keepalive";
            std::map<std::string, std::string> data;
            data["client_uid"] = "0001";
            const std::string msg = doMsgAssembly(event, data);
            if (msgHub) {
                msgHub->asyncSendMsg(msg);
            }
        }
    }
};

#endif // MASTER_CONNECTOR_HPP
