#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <map>
#include <string>
#include "../Engine/Engine.hpp"
#include "../TcpSession/TcpSession.hpp"

class Scheduler {
public:
    explicit Scheduler(const std::map<std::string, std::string> &config_, TcpSession &session_)
        : config(config_), session(session_) {
        // 查询布隆过滤器默认设置
        std::map<std::string, std::string> data;
        data["client_uid"] = config.at("client_uid");
        session.asyncSendMsg(doMsgAssembly(std::string("get_bloom_filter_default_config"), data));

        // 接收布隆过滤器默认设置
        std::string event;
        std::map<std::string, std::string> data_;
        doMsgParse(session.recvMsg(), event, data_);

        // 解析布隆过滤器默认设置
        if (event == "bloom_filter_default_config") {
            const unsigned int maxJwtLifeTime = stringToUInt(data_.at("max_jwt_life_time"));
            const unsigned int rotationInterval = stringToUInt(data_.at("rotation_interval"));
            const size_t bloomFilterSize = stringToSizeT(data_.at("bloom_filter_size"));
            const unsigned int hashFunctionNum = stringToUInt(data_.at("hash_function_num"));

            // 初始化引擎
            std::cout << std::endl << "Initializing Bloom filter engine..." << std::endl;
            engine = new Engine(config, maxJwtLifeTime, rotationInterval, bloomFilterSize, hashFunctionNum);
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

        // 启动节点上报线程
        if (!nodeStatusReportThread.joinable()) {
            nodeStatusReportThreadRunFlag.store(true);
            nodeStatusReportThread = std::thread(&Scheduler::nodeStatusReportWorker, this,
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

        // 停止节点状态上报线程
        nodeStatusReportThreadRunFlag.store(false);
        if (nodeStatusReportThread.joinable()) nodeStatusReportThread.join();

        // 清理引擎
        free(engine);
    }

    [[nodiscard]] Engine *getEngine() const { return engine; }

private:
    const std::map<std::string, std::string> &config;
    TcpSession &session;

    // 引擎
    Engine *engine = nullptr;

    // 处理消息线程
    std::atomic<bool> msgProcThreadRunFlag{false};
    std::thread msgProcThread;

    void msgProcWorker() const {
        while (msgProcThreadRunFlag) {
            std::string msg = session.recvMsg();
            std::string event;
            std::map<std::string, std::string> data;
            doMsgParse(msg, event, data);

            if (event == "revoke_jwt") {
                const std::string token = data["token"];
                const std::string expTime = data["exp_time"];
                engine->revokeJwt(token, stringToTimestamp(expTime));
                engine->logRevoke(token, stringToTimestamp(expTime));
                std::cout << "[revoke_jwt] " << token << std::endl;
                continue;
            }

            // 调整参数，并重建布隆过滤器
            if (event == "adjust_bloom_filter") {
            }
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
            data["client_uid"] = config.at("client_uid");
            const std::string msg = doMsgAssembly(event, data);
            session.asyncSendMsg(msg);
        }
    }

    // 节点状态上报线程
    std::atomic<bool> nodeStatusReportThreadRunFlag{false};
    std::thread nodeStatusReportThread;

    void nodeStatusReportWorker(const unsigned int interval) const {
        while (nodeStatusReportThreadRunFlag) {
            std::this_thread::sleep_for(std::chrono::seconds(interval));
            const std::string event = "node_status";
            std::map<std::string, std::string> data;
            data["client_uid"] = config.at("client_uid");
            data["max_jwt_life_time"] = std::to_string(engine->getMaxJwtLifeTime()); // T^max_i
            data["rotation_interval"] = std::to_string(engine->getRotationInterval()); // T^w_i
            data["bloom_filter_size"] = std::to_string(engine->getBloomFilterSize()); // m^bf_i
            data["hash_function_num"] = std::to_string(engine->getHashFunctionNum()); // k^hash_i
            data["bloom_filter_num"] = std::to_string(engine->getBloomFilterNum()); // n^bf_i
            data["black_list_msg_num"] = vectorToString(engine->getBlackListMsgNum()); // n^jwt_(i-1,j)
            const std::string msg = doMsgAssembly(event, data);
            session.asyncSendMsg(msg);
        }
    }
};

#endif //SCHEDULER_HPP
