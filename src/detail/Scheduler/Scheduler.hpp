#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <map>
#include <string>
#include "../Engine/Engine.hpp"
#include "../MasterSession/MasterSession.hpp"
#include "../Utils/JsonSerializer.hpp"
#include "NodeMessageSender.hpp"

class Scheduler {
public:
    explicit Scheduler(const std::map<std::string, std::string> &config_, MasterSession &session_, Engine &engine_)
        : config(config_), session(session_), engine(engine_) {
        // 查询布隆过滤器默认设置
        std::map<std::string, std::string> data;
        data["client_uid"] = config.at("client_uid");
        session.asyncSendMsg(msgAssembly(std::string("get_bloom_filter_default_config"), data));

        // 接收布隆过滤器默认设置
        std::string event;
        std::map<std::string, std::string> data_;
        msgParse(session.recvMsg(), event, data_);

        // 解析布隆过滤器默认设置
        if (event == "bloom_filter_default_config") {
            const unsigned int maxJwtLifeTime = stringToUInt(data_.at("max_jwt_life_time"));
            const unsigned int rotationInterval = stringToUInt(data_.at("rotation_interval"));
            const size_t bloomFilterSize = stringToSizeT(data_.at("bloom_filter_size"));
            const unsigned int hashFunctionNum = stringToUInt(data_.at("hash_function_num"));

            std::cout << "[Scheduler] " << "maxJwtLifeTime: " << maxJwtLifeTime << ", rotationInterval: " <<
                    rotationInterval << ", bloomFilterSize: " << bloomFilterSize << ", hashFunctionNum: " <<
                    hashFunctionNum << std::endl;

            std::cout << "[Scheduler] " << "Bloom filter memory used: " << std::ceil(maxJwtLifeTime / rotationInterval)
                    * static_cast<unsigned long>(bloomFilterSize) / 8388608 << " MBytes" << std::endl;

            // 初始化引擎
            engine.init(maxJwtLifeTime, rotationInterval, bloomFilterSize, hashFunctionNum);
        } else throw std::runtime_error("Get bloom filter default config failed.");

        // 启动处理消息线程
        if (!msgProcThread.joinable()) {
            msgProcThreadRunFlag.store(true);
            msgProcThread = std::thread(&Scheduler::msgProcWorker, this);
        }

        // 启动发送心跳包线程
        if (!keepaliveThread.joinable()) {
            keepaliveThreadRunFlag.store(true);
            keepaliveThread = std::thread(&Scheduler::keepaliveWorker, this,
                                          stringToUInt(config.at("keepalive_interval")));
        }

        // 启动布隆过滤器状态上报线程
        if (!bloomFilterStatusReportThread.joinable()) {
            bloomFilterStatusReportRunFlag.store(true);
            bloomFilterStatusReportThread = std::thread(&Scheduler::bloomFilterStatusReport, this,
                                                        stringToUInt(config.at("node_status_report_interval")));
        }
    }

    ~Scheduler() {
        // 停止处理消息线程
        msgProcThreadRunFlag.store(false);
        if (msgProcThread.joinable()) msgProcThread.join();

        // 停止发送心跳包线程
        keepaliveThreadRunFlag.store(false);
        if (keepaliveThread.joinable()) keepaliveThread.join();

        // 停止布隆过滤器状态上报线程
        bloomFilterStatusReportRunFlag.store(false);
        if (bloomFilterStatusReportThread.joinable()) bloomFilterStatusReportThread.join();
    }

    // 代理查询
    bool proxyQuery(const std::string &token, const time_t &expTime) {
        return nodeMessageSender.isRevoked(token, std::to_string(expTime));
    }

    std::string getNodeRole() {
        return nodeRole;
    }

private:
    const std::map<std::string, std::string> &config;
    MasterSession &session;
    Engine &engine;
    NodeMessageSender nodeMessageSender = NodeMessageSender();
    std::string nodeRole = "single_node";

    // 处理消息线程
    std::atomic<bool> msgProcThreadRunFlag{false};
    std::thread msgProcThread;

    void msgProcWorker() {
        while (msgProcThreadRunFlag) {
            std::string msg = session.recvMsg();
            std::string event;
            std::map<std::string, std::string> data;
            msgParse(msg, event, data);

            if (event == "revoke_jwt") {
                const std::string token = data["token"];
                const std::string expTime = data["exp_time"];

                if (nodeRole == "single_node" || nodeRole == "proxy_node") {
                    // 如果 `node_role` 是 `single_node` 或 `proxy_node`，则在自己的布隆过滤器中撤回
                    engine.revokeJwt(token, stringToTimestamp(expTime));
                } else if (nodeRole == "slave_node") {
                    // 如果是 `slave_node`，则将jwt发送给 proxy_node 撤回
                    nodeMessageSender.revokeJwt(token, expTime);
                }
                engine.logRevoke(token, stringToTimestamp(expTime)); // 不管是什么模式，都要写日志
                std::cout << "[revoke_jwt][" << nodeRole << "] " << token << std::endl;
                continue;
            }

            // 调整参数，更改服务器角色，并重建布隆过滤器
            if (event == "adjust_bloom_filter") {
                const std::string node_role = data.at("node_role");

                // single_node 逻辑
                if (node_role == "single_node") {
                    nodeRole = node_role;
                    nodeMessageSender.disconnect();
                    const unsigned int maxJwtLifeTime = stringToUInt(data.at("max_jwt_life_time"));
                    const unsigned int rotationInterval = stringToUInt(data.at("rotation_interval"));
                    const size_t bloomFilterSize = stringToSizeT(data.at("bloom_filter_size"));
                    const unsigned int hashFunctionNum = stringToUInt(data.at("hash_function_num"));
                    // 调整
                    engine.adjustFiltersParam(maxJwtLifeTime, rotationInterval, bloomFilterSize, hashFunctionNum);
                    // 回执
                    std::map<std::string, std::string> data_;
                    data_["node_uid"] = config.at("client_uid");
                    data_["uuid"] = data.at("uuid");
                    data_["node_role"] = node_role;
                    session.asyncSendMsg(msgAssembly("adjust_bloom_filter_done", data_));
                    // 打印
                    std::cout << "[Scheduler] " << "nodeMode: " << nodeRole << " maxJwtLifeTime: " << maxJwtLifeTime <<
                            ", rotationInterval: " << rotationInterval << ", bloomFilterSize: " << bloomFilterSize <<
                            ", hashFunctionNum: " << hashFunctionNum << std::endl;
                    continue;
                }

                // proxy_node 逻辑
                if (node_role == "proxy_node") {
                    nodeRole = node_role;
                    nodeMessageSender.disconnect();
                    const unsigned int maxJwtLifeTime = stringToUInt(data.at("max_jwt_life_time"));
                    const unsigned int rotationInterval = stringToUInt(data.at("rotation_interval"));
                    const size_t bloomFilterSize = stringToSizeT(data.at("bloom_filter_size"));
                    const unsigned int hashFunctionNum = stringToUInt(data.at("hash_function_num"));
                    // 调整
                    engine.adjustFiltersParam(maxJwtLifeTime, rotationInterval, bloomFilterSize, hashFunctionNum);
                    // 回执
                    std::map<std::string, std::string> data_;
                    data_["node_uid"] = config.at("client_uid");
                    data_["uuid"] = data.at("uuid");
                    data_["node_role"] = node_role;
                    session.asyncSendMsg(msgAssembly("adjust_bloom_filter_done", data_));
                    // 打印
                    std::cout << "[Scheduler] " << "nodeMode: " << nodeRole << " maxJwtLifeTime: " << maxJwtLifeTime <<
                            ", rotationInterval: " << rotationInterval << ", bloomFilterSize: " << bloomFilterSize <<
                            ", hashFunctionNum: " << hashFunctionNum << std::endl;
                    continue;
                }

                // single_node 逻辑
                if (node_role == "slave_node") {
                    nodeRole = node_role;
                    nodeMessageSender.disconnect();
                    // 启动TCP客户端，将 log 发送给 proxy_node
                    const std::string proxy_node_host = data.at("proxy_node_host");
                    const std::string proxy_node_port = data.at("proxy_node_port");
                    nodeMessageSender.connect(proxy_node_host, stringToUShort(proxy_node_port));
                    nodeMessageSender.sendLogToProxyNode(config.at("log_file_path"));
                    // 调整
                    engine.adjustFiltersParam(86400, 86400, 8, 1);
                    // 回执
                    std::map<std::string, std::string> data_;
                    data_["node_uid"] = config.at("client_uid");
                    data_["uuid"] = data.at("uuid");
                    data_["node_role"] = node_role;
                    session.asyncSendMsg(msgAssembly("adjust_bloom_filter_done", data_));
                    // 打印
                    std::cout << "[Scheduler] " << "nodeMode: " << nodeRole << std::endl;
                }
                continue;
            }
        }
    }

    // 发送心跳包线程
    std::atomic<bool> keepaliveThreadRunFlag{false};
    std::thread keepaliveThread;

    void keepaliveWorker(const unsigned int interval) const {
        while (keepaliveThreadRunFlag) {
            const std::string event = "keepalive";
            std::map<std::string, std::string> data;
            data["client_uid"] = config.at("client_uid");
            data["node_port"] = config.at("server_port");
            const std::string msg = msgAssembly(event, data);
            session.asyncSendMsg(msg);
            std::this_thread::sleep_for(std::chrono::seconds(interval));
        }
    }

    // 布隆过滤器状态上报线程
    std::atomic<bool> bloomFilterStatusReportRunFlag{false};
    std::thread bloomFilterStatusReportThread;

    void bloomFilterStatusReport(const unsigned int interval) const {
        while (bloomFilterStatusReportRunFlag) {
            std::this_thread::sleep_for(std::chrono::seconds(interval));
            const std::string event = "bloom_filter_status";
            std::map<std::string, std::string> data;
            data["client_uid"] = config.at("client_uid");
            data["max_jwt_life_time"] = std::to_string(engine.getMaxJwtLifeTime()); // T^max_i
            data["rotation_interval"] = std::to_string(engine.getRotationInterval()); // T^w_i
            data["bloom_filter_size"] = std::to_string(engine.getBloomFilterSize()); // m^bf_i
            data["hash_function_num"] = std::to_string(engine.getHashFunctionNum()); // k^hash_i
            data["bloom_filter_filling_rate"] = vectorToString(engine.getBloomFilterFillingRate()); // n^jwt_(i-1,j)
            const std::string msg = msgAssembly(event, data);
            session.asyncSendMsg(msg);
        }
    }
};

#endif //SCHEDULER_HPP
