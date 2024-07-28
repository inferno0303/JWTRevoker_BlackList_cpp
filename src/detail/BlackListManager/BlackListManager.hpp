#ifndef BLOOM_FILTER_MANAGER_HPP
#define BLOOM_FILTER_MANAGER_HPP

#include <iostream>
#include <memory>
#include <thread>
#include <atomic>
#include <future>
#include <memory>

#include "../Network/NetworkUtils/MsgFormat.hpp"
#include "../Network/NetworkUtils/NioTcpMsgBridge.hpp"
#include "../BlackListEngine/BlackListEngine.hpp"
#include "../Utils/StringConverter.hpp"

class BlackListManager {
public:
    explicit BlackListManager(NioTcpMsgBridge& bridge, const std::map<std::string, std::string>& config) :
        msgBridge(bridge), startupConfig(config) {
        // 启动处理消息线程
        if (!processMsgThread.joinable()) {
            processMsgThreadRunFlag.store(true);
            processMsgThread = std::thread(&BlackListManager::processMsgWorker, this);
        }

        // 发送 get_bloom_filter_default_config 消息，阻塞等待回应
        std::map<std::string, std::string> bfConfig = this->awaitBFDefaultConfig(); // 阻塞并等待一个async函数返回结果

        const std::string maxJwtLifeTime = bfConfig["max_jwt_life_time"];
        const std::string bloomFilterRotationTime = bfConfig["bloom_filter_rotation_time"];
        const std::string bloomFilterSize = bfConfig["bloom_filter_size"];
        const std::string numHashFunction = bfConfig["num_hash_function"];

        std::cout << "Allocation bloom filter memory..." << std::endl;

        // 黑名单初始化，分配内存
        try {
            this->engine = std::make_shared<BlackListEngine>(
                stringToUInt(maxJwtLifeTime),
                stringToUInt(bloomFilterRotationTime),
                stringToSizeT(bloomFilterSize),
                stringToUInt(numHashFunction)
            );
            std::cout << "Bloom filter memory allocation success!" << std::endl;
        } catch (const std::bad_alloc& e) {
            std::cerr << "Memory allocation failed: " << e.what() << std::endl;
            throw std::runtime_error("Memory allocation failed!");
        } catch (const std::exception& e) {
            std::cerr << "An exception occurred: " << e.what() << std::endl;
            throw std::runtime_error("Memory allocation failed!");
        }

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

    std::shared_ptr<BlackListEngine> getEngine() const {
        return engine;
    }

private:
    // 与 master 服务器的消息桥
    NioTcpMsgBridge& msgBridge;

    // 程序启动配置
    std::map<std::string, std::string> startupConfig;

    // 引擎
    std::shared_ptr<BlackListEngine> engine = nullptr;

    // 发送 get_bloom_filter_default_config 消息，查询布隆过滤器默认配置
    std::promise<std::map<std::string, std::string>> getBFDefaultConfigPromise;

    std::map<std::string, std::string> awaitBFDefaultConfig() {
        const std::string event = "get_bloom_filter_default_config";
        std::map<std::string, std::string> data;
        data["client_uid"] = "0001";
        const std::string msg = doMsgAssembly(event, data);
        msgBridge.asyncSendMsg(msg);

        // 等待回应
        return getBFDefaultConfigPromise.get_future().get();
    }

    // 处理消息线程
    std::atomic<bool> processMsgThreadRunFlag{false};
    std::thread processMsgThread;

    void processMsgWorker() {
        while (processMsgThreadRunFlag) {
            // 从队列中接收消息（队列空则阻塞）
            const std::string msg = msgBridge.recvMsg();
            std::string event;
            std::map<std::string, std::string> data;
            doMsgParse(msg, event, data);

            // 处理事件
            if (event == "revoke") {
                const std::string token = data["token"];
                const std::string expTime = data["exp_time"];
                this->engine->write(token, stringToTimestamp(expTime));
                continue;
            }

            if (event == "bloom_filter_default_config") {
                this->getBFDefaultConfigPromise.set_value(data);
                continue;
            }
            std::cout << "Unknow event: " << event << std::endl;
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
            data["max_jwt_life_time"] = std::to_string(engine->getMaxJwtLifeTime()); // T^max_i
            data["rotation_interval"] = std::to_string(engine->getRotationInterval()); // T^w_i
            data["bloom_filter_num"] = std::to_string(engine->getBloomFilterNum()); // n^bf_i
            data["black_list_msg_num"] = vectorToString(engine->getBlackListMsgNum()); // n^jwt_(i-1,j)
            data["bloom_filter_size"] = std::to_string(engine->getBloomFilterSize()); // m^bf_i
            data["hash_function_num"] = std::to_string(engine->getHashFunctionNum()); // k^hash_i

            const std::string msg = doMsgAssembly(event, data);
            msgBridge.asyncSendMsg(msg);
        }
    }
};


#endif // BLOOM_FILTER_MANAGER_HPP
