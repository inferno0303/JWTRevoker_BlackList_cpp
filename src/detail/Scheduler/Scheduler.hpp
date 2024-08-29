#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <map>
#include <string>
#include <future>

#include "../Engine/Engine.hpp"
#include "../MasterConnector/MasterConnector.hpp"

class Scheduler {
public:
    explicit Scheduler(const std::map<std::string, std::string>& _config, MasterConnector& _conn)
        : config(_config), conn(_conn) {
        // 启动处理消息线程
        if (!msgProcessThread.joinable()) {
            msgProcessThreadRunFlag.store(true);
            msgProcessThread = std::thread(&Scheduler::msgProcessWorker, this);
        }

        // 查询布隆过滤器默认配置
        const std::map<std::string, std::string> bfDefaultConfig = this->getBFDefaultConfig();

        // 解析默认配置
        const unsigned int maxJwtLifeTime = stringToUInt(bfDefaultConfig.at("max_jwt_life_time"));
        const unsigned int rotationInterval = stringToUInt(bfDefaultConfig.at("rotation_interval"));
        const size_t bloomFilterSize = stringToSizeT(bfDefaultConfig.at("bloom_filter_size"));
        const unsigned int hashFunctionNum = stringToUInt(bfDefaultConfig.at("hash_function_num"));

        // 初始化引擎
        std::cout << "\nInitialize Bloom filter engine..." << std::endl;
        engine = new Engine(config, maxJwtLifeTime, rotationInterval, bloomFilterSize, hashFunctionNum);

        // 启动节点上报线程
        if (!nodeStatusReportThread.joinable()) {
            nodeStatusReportThreadRunFlag.store(true);
            nodeStatusReportThread = std::thread(&Scheduler::nodeStatusReportWorker, this,
                                                 stringToUInt(config.at("node_status_report_interval")));
        }
    }

    ~Scheduler() {
        // 停止处理消息线程
        msgProcessThreadRunFlag.store(false);
        if (msgProcessThread.joinable()) { msgProcessThread.join(); }

        // 停止节点状态上报线程
        nodeStatusReportThreadRunFlag.store(false);
        if (nodeStatusReportThread.joinable()) { nodeStatusReportThread.join(); }

        // 清理引擎
        free(engine);
    }

    Engine* getEngine() const { return engine; }

private:
    // 配置
    const std::map<std::string, std::string>& config;

    // master服务器连接
    MasterConnector& conn;

    // 引擎
    Engine* engine = nullptr;

    // 处理消息线程
    std::atomic<bool> msgProcessThreadRunFlag{false};
    std::thread msgProcessThread;

    void msgProcessWorker() {
        while (msgProcessThreadRunFlag) {
            std::string msg = conn.recvMsg();
            std::string event;
            std::map<std::string, std::string> data;
            doMsgParse(msg, event, data);

            // 撤回
            if (event == "revoke_jwt") {
                const std::string token = data["token"];
                const std::string expTime = data["exp_time"];
                if (engine) { engine->write(token, stringToTimestamp(expTime)); }
                continue;
            }

            // 调整参数，并重建布隆过滤器

            // 布隆过滤器默认配置
            if (event == "bloom_filter_default_config") {
                getBFDefaultConfigProm.set_value(data);
                continue;
            }

            // std::cout << event << std::endl;
        }
    }

    // 发送 get_bloom_filter_default_config 消息，查询布隆过滤器默认配置
    std::promise<std::map<std::string, std::string>> getBFDefaultConfigProm;

    std::map<std::string, std::string> getBFDefaultConfig() {
        const std::string event = "get_bloom_filter_default_config";
        std::map<std::string, std::string> data;
        data["client_uid"] = config.at("client_uid");
        const std::string msg = doMsgAssembly(event, data);
        conn.asyncSendMsg(msg);
        return getBFDefaultConfigProm.get_future().get();
    }

    // 节点状态上报线程
    std::atomic<bool> nodeStatusReportThreadRunFlag{false};
    std::thread nodeStatusReportThread;

    void nodeStatusReportWorker(const unsigned int interval) const {
        while (nodeStatusReportThreadRunFlag) {
            std::this_thread::sleep_for(std::chrono::seconds(interval));

            // 收集节点状态
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
            conn.asyncSendMsg(msg);
        }
    }
};

#endif //SCHEDULER_HPP
