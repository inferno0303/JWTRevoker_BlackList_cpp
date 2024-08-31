#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <map>
#include <boost/asio.hpp>
#include "../Engine/Engine.hpp"
#include "CoroutineSafeQueue.hpp"
#include "../Utils/JsonStringMsg.hpp"
#include "../Utils/StringParser.hpp"

using boost::asio::io_context;
using boost::asio::awaitable;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;

inline awaitable<std::string> asyncRecvMsgFromSocket(tcp::socket &sock) {
    // 接收消息头
    char msgHeaderBE[4]{};
    co_await async_read(sock, boost::asio::buffer(msgHeaderBE, 4), use_awaitable);

    // 网络字节序转为主机字节序
    std::uint32_t msgBodyLength = 0;
    std::memcpy(&msgBodyLength, msgHeaderBE, 4);
    msgBodyLength = ntohl(msgBodyLength);
    if (msgBodyLength == 0) co_return std::string();

    std::vector<char> msgBody(msgBodyLength);
    co_await async_read(sock, boost::asio::buffer(msgBody.data(), msgBodyLength), use_awaitable);

    co_return std::string(msgBody.begin(), msgBody.end());
}

inline awaitable<void> asyncSendMsgToSocket(tcp::socket &sock, const std::string &msg) {
    if (msg.empty()) co_return;
    // 动态分配消息帧内存，包括 4 bytes 的消息长度和消息体
    std::vector<char> msgFrame(4 + msg.size());
    const auto msgLengthBE = htonl(msg.size());
    std::memcpy(msgFrame.data(), &msgLengthBE, 4);
    std::memcpy(msgFrame.data() + 4, msg.data(), msg.size());

    // 异步发送消息帧
    co_await async_write(sock, boost::asio::buffer(msgFrame));
}


class Server {
public:
    Server(const std::map<std::string, std::string> &config_, Engine &engine_) : config(config_), engine(engine_) {
    }

    ~Server() = default;

    void run() const {
        try {
            io_context io_context(1);
            boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
            signals.async_wait([&](auto, auto) { io_context.stop(); });
            co_spawn(io_context, listener(io_context), boost::asio::detached);
            io_context.run();
        } catch (std::exception &e) {
            std::printf("Exception: %s\n", e.what());
        }
    };

private:
    const std::map<std::string, std::string> &config;
    Engine &engine;

    awaitable<void> listener(io_context &ioc) const {
        auto server_port = stringToUShort(config.at("server_port")); // 读取配置文件的端口号
        auto endpoint = tcp::endpoint({tcp::v4(), server_port});
        tcp::acceptor acceptor(ioc, endpoint);
        std::cout << "Server is running at: " << endpoint << std::endl;
        while (true) {
            tcp::socket socket = co_await acceptor.async_accept(use_awaitable);
            co_spawn(ioc, handleClient(std::move(socket), ioc), boost::asio::detached);
        }
    }

    awaitable<void> handleClient(tcp::socket sock, io_context &ioc) const {
        std::cout << "New client is connected: " << sock.remote_endpoint() << std::endl;
        auto recvQueue = CoroutineSafeQueue<std::string>(ioc);
        auto sendQueue = CoroutineSafeQueue<std::string>(ioc);
        try {
            auto recv = co_spawn(ioc, recvTask(sock, recvQueue), use_awaitable);
            auto send = co_spawn(ioc, sendTask(sock, sendQueue), use_awaitable);
            auto process = co_spawn(ioc, processTask(recvQueue, sendQueue, engine), use_awaitable);
            co_await std::move(recv);
            co_await std::move(send);
            co_await std::move(process);
        } catch (const std::exception &e) {
            // 如果任一协程抛出异常，其他协程也会被取消
            std::cerr << e.what() << std::endl;
            std::cout << "Client connection is lost: " << sock.remote_endpoint() << std::endl;
        }
        co_return;
    }

    static awaitable<void> recvTask(tcp::socket &sock, CoroutineSafeQueue<std::string> &recvQueue) {
        while (true) {
            recvQueue.enqueue(co_await asyncRecvMsgFromSocket(sock));
        }
    }

    static awaitable<void> sendTask(tcp::socket &sock, CoroutineSafeQueue<std::string> &sendQueue) {
        while (true) {
            co_await asyncSendMsgToSocket(sock, co_await sendQueue.dequeue());
        }
    }

    awaitable<void> processTask(CoroutineSafeQueue<std::string> &recvQueue,
                                CoroutineSafeQueue<std::string> &sendQueue,
                                Engine &engine) const {
        while (true) {
            auto message = co_await recvQueue.dequeue();
            std::string event;
            std::map<std::string, std::string> data;
            msgParse(message, event, data);

            // 被客户端查询
            if (event == "is_jwt_revoked") {
                const std::string token = data["token"];
                const std::string expTime = data["exp_time"];
                const bool isRevoked = engine.isRevoked(token, stringToTimestamp(expTime));

                std::map<std::string, std::string> data_;
                data_["token"] = token;
                data_["expTime"] = expTime;
                data_["status"] = isRevoked ? "revoked" : "active";
                const std::string resp = msgAssembly("is_jwt_revoked_response", data_);
                sendQueue.enqueue(resp);
                continue;
            }

            // 集群网络中的其他布隆过滤器被设置为 `委托节点`，要向当前节点加载历史撤回记录
            if (event == "get_revoke_log") {
                const std::string hourlyTimestamp = data["hourly_timestamp"]; // 接收整点时间戳
                std::filesystem::path filePath = config.at("log_file_path");
                filePath /= hourlyTimestamp + ".txt"; // 拼接路径

                // 判断文件是否存在
                bool isDone = false;
                if (!exists(filePath)) isDone = true;
                std::ifstream file(filePath);
                if (!file.is_open()) isDone = true;
                if (isDone) {
                    // 如果读取文件出错，则直接回复已完成
                    std::map<std::string, std::string> data_;
                    data_["hourly_timestamp"] = hourlyTimestamp;
                    const std::string resp = msgAssembly("get_revoke_log_done", data_);
                    sendQueue.enqueue(resp);
                    continue;
                }

                // 逐行读取和发送
                std::string line;
                while (std::getline(file, line)) {
                    std::istringstream iss(line);
                    if (std::string token; std::getline(iss, token, ',')) {
                        // 解析 token 字符串
                        if (std::string expTimeStr; std::getline(iss, expTimeStr)) {
                            // 解析 expTime（字符串），只发送没有自然过期的记录
                            if (static_cast<time_t>(std::stol(expTimeStr)) >= std::chrono::system_clock::to_time_t(
                                    std::chrono::system_clock::now())) {
                                // 发送消息
                                std::map<std::string, std::string> data_;
                                data_["token"] = token;
                                data_["expTime"] = expTimeStr;
                                const std::string resp = msgAssembly("get_revoke_log_response", data_);
                                sendQueue.enqueue(resp);
                            }
                        }
                    }
                }
                std::map<std::string, std::string> data_;
                data_["hourly_timestamp"] = hourlyTimestamp;
                const std::string resp = msgAssembly("get_revoke_log_done", data_);
                sendQueue.enqueue(resp);
                continue;
            }
        }
    }
};

#endif //TCP_SERVER_H
