#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <map>
#include <boost/asio.hpp>
#include "../Engine/Engine.hpp"
#include "../Scheduler/Scheduler.hpp"
#include "CoroutineSafeQueue.hpp"
#include "../Utils/JsonSerializer.hpp"
#include "../Utils/StringParser.hpp"
#include "../Utils/SocketMsgFrame.hpp"

using boost::asio::io_context;
using boost::asio::awaitable;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;

class Server {
public:
    Server(const std::map<std::string, std::string> &config_, Engine &engine_, Scheduler &scheduler_)
        : config(config_), engine(engine_), scheduler(scheduler_) {
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
    Scheduler &scheduler;

    awaitable<void> listener(io_context &ioc) const {
        auto server_port = stringToUShort(config.at("server_port")); // 读取配置文件的端口号
        auto endpoint = tcp::endpoint({tcp::v4(), server_port});
        tcp::acceptor acceptor(ioc, endpoint);
        std::cout << "Server is running at: " << endpoint << std::endl << std::endl;
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
            auto process = co_spawn(ioc, processTask(recvQueue, sendQueue), use_awaitable);
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
                                CoroutineSafeQueue<std::string> &sendQueue) const {
        while (true) {
            auto message = co_await recvQueue.dequeue();
            std::string event;
            std::map<std::string, std::string> data;
            msgParse(message, event, data);

            // 查询请求
            if (event == "is_jwt_revoked") {
                const std::string token = data["token"];
                const std::string expTime = data["exp_time"];
                // 如果是 single_node 或 proxy_node 模式，则查询自身的布隆过滤器
                if (scheduler.getNodeRole() == "single_node" || scheduler.getNodeRole() == "proxy_node") {
                    const bool isRevoked = engine.isRevoked(token, stringToTimestamp(expTime));
                    std::map<std::string, std::string> data_;
                    data_["token"] = token;
                    data_["expTime"] = expTime;
                    data_["status"] = isRevoked ? "revoked" : "active";
                    const std::string resp = msgAssembly("is_jwt_revoked_response", data_);
                    sendQueue.enqueue(resp);
                    continue;
                }
                // 如果是salve_node，则委托 proxy_node 查询（代理查询）
                if (scheduler.getNodeRole() == "slave_node") {
                    const bool isRevoked = scheduler.proxyQuery(token, stringToTimestamp(expTime));
                    std::map<std::string, std::string> data_;
                    data_["token"] = token;
                    data_["expTime"] = expTime;
                    data_["status"] = isRevoked ? "revoked" : "active";
                    const std::string resp = msgAssembly("is_jwt_revoked_response", data_);
                    sendQueue.enqueue(resp);
                    continue;
                }
            }

            // 当前节点被设置为 `proxy_node` 时，接受其他节点的插入请求
            if (event == "revoke_jwt" && scheduler.getNodeRole() == "proxy_node") {
                const std::string token = data["token"];
                const std::string expTime = data["exp_time"];
                engine.revokeJwt(token, stringToTimestamp(expTime));
                continue;
            }
        }
    }
};

#endif //TCP_SERVER_H
