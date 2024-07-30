#ifndef BLOOM_FILTER_MANAGER_HPP
#define BLOOM_FILTER_MANAGER_HPP

#include <iostream>
#include <thread>
#include <atomic>
#include <future>

#include "../Utils/StringParser.hpp"
#include "../Utils/NetworkUtils/MsgFormat.hpp"
#include "../Utils/NetworkUtils/NioTcpMsgBridge.hpp"

#include "../BlackListEngine/BlackListEngine.hpp"


class BlackListManager {
public:
    explicit BlackListManager(
        BlackListEngine& engine,
        NioTcpMsgBridge* bridge,
        const std::map<std::string, std::string>& config) :
        startupConfig(config),
        msgBridge(bridge),
        engine(engine) {
        // 启动处理消息线程
        if (!processMsgThread.joinable()) {
            processMsgThreadRunFlag.store(true);
            processMsgThread = std::thread(&BlackListManager::processMsgWorker, this);
        }

        // 发送 get_bloom_filter_default_config 消息，阻塞等待回应
        std::map<std::string, std::string> bfConfig = this->awaitBFDefaultConfig();
        const std::string maxJwtLifeTime = bfConfig["max_jwt_life_time"];
        const std::string bloomFilterRotationTime = bfConfig["bloom_filter_rotation_time"];
        const std::string bloomFilterSize = bfConfig["bloom_filter_size"];
        const std::string numHashFunction = bfConfig["num_hash_function"];

        // 引擎初始化
        std::cout << "Allocation bloom filter memory..." << std::endl;
        engine.start(stringToUInt(maxJwtLifeTime), stringToUInt(bloomFilterRotationTime),
                     stringToSizeT(bloomFilterSize), stringToUInt(numHashFunction));
        std::cout << "Bloom filter is ready!" << std::endl;

        // 启动节点状态上报线程
        if (!nodeStatusReportThread.joinable()) {
            nodeStatusReportThreadRunFlag.store(true);
            nodeStatusReportThread = std::thread(&BlackListManager::nodeStatusReportWorker, this);
        }
    }

    ~BlackListManager() {
        // 停止处理消息线程
        processMsgThreadRunFlag.store(false);
        if (processMsgThread.joinable()) {
            processMsgThread.join();
        }

        // 停止节点状态上报线程
        nodeStatusReportThreadRunFlag.store(false);
        if (nodeStatusReportThread.joinable()) {
            nodeStatusReportThread.join();
        }
    }

private:
    // 配置
    std::map<std::string, std::string> startupConfig;

    // 消息桥
    NioTcpMsgBridge* msgBridge = nullptr;

    // 引擎
    BlackListEngine& engine;

    // 发送 get_bloom_filter_default_config 消息，查询布隆过滤器默认配置
    std::promise<std::map<std::string, std::string>> getBFDefaultConfigPromise;

    std::map<std::string, std::string> awaitBFDefaultConfig() {
        const std::string event = "get_bloom_filter_default_config";
        std::map<std::string, std::string> data;
        data["client_uid"] = "0001";
        const std::string msg = doMsgAssembly(event, data);
        msgBridge->asyncSendMsg(msg);

        // 等待回应
        return getBFDefaultConfigPromise.get_future().get();
    }

    // 处理消息线程
    std::atomic<bool> processMsgThreadRunFlag{false};
    std::thread processMsgThread;

    void processMsgWorker() {
        while (processMsgThreadRunFlag) {
            // 从队列中接收消息（队列空则阻塞）
            const std::string msg = msgBridge->recvMsg();
            std::string event;
            std::map<std::string, std::string> data;
            doMsgParse(msg, event, data);

            // 处理事件
            if (event == "revoke") {
                const std::string token = data["token"];
                const std::string expTime = data["exp_time"];
                engine.write(token, stringToTimestamp(expTime));
                continue;
            }

            if (event == "bloom_filter_default_config") {
                this->getBFDefaultConfigPromise.set_value(data);
                continue;
            }
            std::cout << "Unknown event: " << event << std::endl;
        }
    }

    // 节点状态上报线程
    std::atomic<bool> nodeStatusReportThreadRunFlag{false};
    std::thread nodeStatusReportThread;

    void nodeStatusReportWorker() {
        const unsigned int interval = stringToUInt(startupConfig["node_status_report_interval"]);
        while (nodeStatusReportThreadRunFlag) {
            std::this_thread::sleep_for(std::chrono::seconds(interval));

            const std::string event = "node_status";
            std::map<std::string, std::string> data;

            // 收集节点状态
            data["client_uid"] = "0001";
            data["max_jwt_life_time"] = std::to_string(engine.getMaxJwtLifeTime()); // T^max_i
            data["rotation_interval"] = std::to_string(engine.getRotationInterval()); // T^w_i
            data["bloom_filter_num"] = std::to_string(engine.getBloomFilterNum()); // n^bf_i
            data["black_list_msg_num"] = vectorToString(engine.getBlackListMsgNum()); // n^jwt_(i-1,j)
            data["bloom_filter_size"] = std::to_string(engine.getBloomFilterSize()); // m^bf_i
            data["hash_function_num"] = std::to_string(engine.getHashFunctionNum()); // k^hash_i

            const std::string msg = doMsgAssembly(event, data);
            msgBridge->asyncSendMsg(msg);
        }
    }
};


#endif // BLOOM_FILTER_MANAGER_HPP
