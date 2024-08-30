#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <boost/asio.hpp>
#include <map>
#include "../Engine/Engine.hpp"
#include "../Utils/MsgFormatter.hpp"
#include "../Utils/StringParser.hpp"

#define RECV_BUFFER_SIZE 4096

using boost::asio::ip::tcp;
using boost::asio::use_awaitable;


class TCPServer {
public:
    TCPServer(const std::map<std::string, std::string> &config_, Engine &engine_) : config(config_), engine(engine_) {
    };

    ~TCPServer() = default;

    void run() const {
        try {
            boost::asio::io_context io_context(1);
            boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
            signals.async_wait([&](auto, auto) { io_context.stop(); });
            co_spawn(io_context, listener(), boost::asio::detached);
            std::cout << "TCP Server is running." << std::endl;
            io_context.run();
        } catch (std::exception &e) {
            std::printf("Exception: %s\n", e.what());
        }
    };

private:
    const std::map<std::string, std::string> &config;
    Engine &engine;

    boost::asio::awaitable<void> listener() const {
        auto executor = co_await boost::asio::this_coro::executor;
        auto server_port = stringToUShort(config.at("server_port")); // 读取配置文件的端口号
        tcp::acceptor acceptor(executor, {tcp::v4(), server_port});
        for (;;) {
            tcp::socket socket = co_await acceptor.async_accept(use_awaitable);
            co_spawn(executor, handleClient(std::move(socket)), boost::asio::detached);
        }
    }

    static boost::asio::awaitable<void> handleClient(tcp::socket socket) {
        try {
            char buffer_[RECV_BUFFER_SIZE]{};
            for (;;) {
                if (const std::size_t n = co_await socket.async_read_some(boost::asio::buffer(buffer_), use_awaitable);
                    n == 0) {
                    std::cout << "Socket closed" << std::endl;
                    socket.close();
                }
                auto msg = std::string(buffer_);
                std::string event;
                std::map<std::string, std::string> data;
                doMsgParse(msg, event, data);
                std::cout << msg << std::endl;
                // co_await async_write(socket, boost::asio::buffer(data, n), use_awaitable);
            }
        } catch (std::exception &e) {
            std::printf("Handle client exception: %s\n", e.what());
        }
    }
};

#endif //TCP_SERVER_H
