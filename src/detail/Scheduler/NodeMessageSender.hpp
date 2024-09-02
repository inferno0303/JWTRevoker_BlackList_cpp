#ifndef NODE_MESSAGE_SENDER_HPP
#define NODE_MESSAGE_SENDER_HPP

#include <iostream>
#include <map>
#include <string>
#include <filesystem>
#include <fstream>
#include <set>
#include <boost/asio.hpp>
#include "../Utils/JsonSerializer.hpp"
#include "../Utils/SocketMsgFrame.hpp"

using boost::asio::io_context;
using boost::asio::awaitable;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;

class NodeMessageSender {
public:
    NodeMessageSender() = default;

    ~NodeMessageSender() {
        if (sock.is_open()) sock.close();
    }

    void connect(const std::string &host, const unsigned short port) {
        tcp::resolver resolver(io_context_);
        const auto endpoints = resolver.resolve(host, std::to_string(port));
        while (true) {
            boost::system::error_code ec;
            const auto connected_endpoint = boost::asio::connect(sock, endpoints, ec);
            if (!ec) {
                std::cout << "[NodeMessageSender] Proxy node connected: " << connected_endpoint << std::endl;
            }
            std::cerr << "[NodeMessageSender] Connection failure, try again after 5 sec: " << ec.message() <<
                    std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5)); // 等待5秒再尝试重新连接
        }
    }

    void sendLogToProxyNode(const std::string &logFilePath) {
        // 计算当前时刻的整点时间戳
        const std::time_t now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm *tm = std::localtime(&now_c);
        tm->tm_min = 0;
        tm->tm_sec = 0;
        std::time_t hourlyTimestamp = std::mktime(tm);

        // 文件路径列表和文件大小列表
        std::vector<std::filesystem::path> foundFiles;
        size_t fileSizes = 0;

        // 循环检查文件是否存在，并获取文件大小
        for (int i = 0; i < 24; ++i) {
            std::filesystem::path filePath = logFilePath;
            filePath /= std::to_string(hourlyTimestamp) + ".txt"; // 拼接路径
            if (exists(filePath)) {
                foundFiles.push_back(filePath);
                fileSizes += file_size(filePath);
            }
            hourlyTimestamp -= 3600; // 减去1小时
        }

        // 逐步导入文件内容
        size_t readBytes = 0;
        for (const auto &it: foundFiles) {
            std::ifstream file(it);
            if (!file.is_open()) {
                std::cerr << "[NodeMessageSender] Error opening file: " << it << std::endl;
                return;
            }
            std::string line;
            while (std::getline(file, line)) {
                std::istringstream iss(line);

                // 解析 token 字符串
                if (std::string token; std::getline(iss, token, ',')) {
                    // 解析 expTime（字符串）
                    if (std::string expTimeStr; std::getline(iss, expTimeStr)) {
                        // 跳过已经自然过期的记录
                        if (auto expTime = static_cast<time_t>(std::stol(expTimeStr));
                            expTime < std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()))
                            continue;

                        // 发送
                        std::map<std::string, std::string> data;
                        data["token"] = token;
                        data["exp_time"] = expTimeStr;
                        const std::string msg = msgAssembly("revoke_jwt", data);
                        sendMsgToSocket(sock, msg);

                        // 显示进度
                        readBytes += 49; // 每行是一条记录，一条记录 49 bytes
                        if (readBytes % 4900000 == 0) {
                            float p = static_cast<float>(readBytes) / static_cast<float>(fileSizes) * 100;
                            std::cout << "[Engine] Recover from log: " << p << "%" << std::endl;
                        }
                    }
                }
            }
            file.close();
        }
        std::cout << "[Engine] Recover from log is done, " << fileSizes / 49 << " items have been loaded." << std::endl;
    }

    // 询问 proxy_node 某个jwt是否被撤回
    bool isRevoked(const std::string &token, const std::string &expTimeStr) {
        std::map<std::string, std::string> data_;
        data_["token"] = token;
        data_["expTime"] = expTimeStr;
        sendMsgToSocket(sock, msgAssembly("is_jwt_revoked", data_));
        // 监听回执
        const std::string reply = recvMsgFromSocket(sock);
        std::string event;
        std::map<std::string, std::string> data;
        msgParse(reply, event, data);
        if (data.at("status") == "revoked") {
            return true;
        }
        if (data.at("status") == "active") {
            return false;
        }
        return true;
    }

    // 将 jwt 发送到 proxy_node 的布隆过滤器
    void revokeJwt(const std::string &token, const std::string &expTimeStr) {
        std::map<std::string, std::string> data;
        data["token"] = token;
        data["exp_time"] = expTimeStr;
        const std::string msg = msgAssembly("revoke_jwt", data);
        sendMsgToSocket(sock, msg);
    }

    void disconnect() {
        if (sock.is_open()) sock.close();
    }

private:
    io_context io_context_;
    tcp::socket sock{io_context_};
};

#endif //NODE_MESSAGE_SENDER_HPP
