#ifndef BLOOM_FILTER_MANAGER_HPP
#define BLOOM_FILTER_MANAGER_HPP

#include <iostream>
#include <thread>
#include <atomic>
#include <future>
#include <memory>

#include "../Network/NetworkUtils/MsgFormat.hpp"
#include "../Network/NetworkUtils/NioTcpMsgBridge.hpp"
#include "../BloomFilterEngine/BloomFilterEngine.h"
#include "../Utils/StringConverter.hpp"

class BloomFilterManager {
public:
    explicit BloomFilterManager(NioTcpMsgBridge* nio_, const std::map<std::string, std::string>& config_) :
        masterServerMsgBridge(nio_), startupConfig(config_) {
        // 启动处理消息线程
        if (!processMsgThread.joinable()) {
            processMsgThreadRunFlag.store(true);
            processMsgThread = std::thread(&BloomFilterManager::processMsgWorker, this);
        }

        // 发送 get_bloom_filter_default_config 消息，阻塞等待回应
        std::map<std::string, std::string> config = this->awaitBFDefaultConfig(); // 阻塞并等待一个async函数返回结果

        const std::string maxJwtLifeTime = config["max_jwt_life_time"];
        const std::string bloomFilterRotationTime = config["bloom_filter_rotation_time"];
        const std::string bloomFilterSize = config["bloom_filter_size"];
        const std::string numHashFunction = config["num_hash_function"];

        std::cout << "Allocation bloom filter memory..." << std::endl;

        // 布隆过滤器初始化，分配内存
        try {
            this->bloomFilterEngine = std::make_shared<BloomFilterEngine>(
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

        // 启动周期轮换线程
        if (!cycleRotationThread.joinable()) {
            cycleRotationThreadRunFlag.store(true);
            cycleRotationThread = std::thread(&BloomFilterManager::cycleRotationWorker, this);
        }

        // 启动节点状态上报线程
        if (!nodeStatusReportThread.joinable()) {
            nodeStatusReportThreadRunFlag.store(true);
            nodeStatusReportThread = std::thread(&BloomFilterManager::nodeStatusReportWorker, this);
        }
    }

    ~BloomFilterManager() {
        // 停止处理消息线程
        processMsgThreadRunFlag.store(false);
        if (processMsgThread.joinable()) {
            processMsgThread.join();
        }

        // 停止周期轮换线程
        cycleRotationThreadRunFlag.store(false);
        if (cycleRotationThread.joinable()) {
            cycleRotationThread.join();
        }

        // 停止节点状态上报线程
        nodeStatusReportThreadRunFlag.store(false);
        if (nodeStatusReportThread.joinable()) {
            nodeStatusReportThread.join();
        }
    };

private:
    // 与 master 服务器的消息桥
    NioTcpMsgBridge* masterServerMsgBridge{};

    // 程序启动配置
    std::map<std::string, std::string> startupConfig;

    // 布隆过滤器指针
    std::shared_ptr<BloomFilterEngine> bloomFilterEngine{};

    // 异步事件
    std::promise<std::map<std::string, std::string>> getBFDefaultConfigPromise;

    // 处理消息线程
    std::atomic<bool> processMsgThreadRunFlag{false};
    std::thread processMsgThread;

    void processMsgWorker() {
        while (processMsgThreadRunFlag) {
            // 从队列中接收消息（队列空则阻塞）
            const char* msg = masterServerMsgBridge->recvMsg();
            std::string event;
            std::map<std::string, std::string> data;
            doMsgParse(std::string(msg), event, data);
            delete[] msg;

            // 处理这些事件
            if (event == "ping_from_server") {
                auto future = std::async(std::launch::async, &BloomFilterManager::replyPongFromClient, this);
                continue;
            }
            if (event == "bloom_filter_default_config") {
                this->getBFDefaultConfigPromise.set_value(data);
                continue;
            }
            std::cout << "Unknow event: " << event << std::endl;
        }
    }

    // 发送 get_bloom_filter_default_config 消息，查询布隆过滤器默认配置
    std::map<std::string, std::string> awaitBFDefaultConfig() {
        const std::string event = "get_bloom_filter_default_config";
        std::map<std::string, std::string> data;
        data["client_uid"] = "0001";
        const std::string msg = doMsgAssembly(event, data);
        masterServerMsgBridge->sendMsg(msg.c_str());

        // 等待回应
        return getBFDefaultConfigPromise.get_future().get();
    }

    // 发送 pong_from_client 消息
    void replyPongFromClient() const {
        const std::string event = "pong_from_client";
        std::map<std::string, std::string> data;
        data["client_uid"] = "0001";
        const std::string msg = doMsgAssembly(event, data);
        masterServerMsgBridge->sendMsg(msg.c_str());
    }

    // 周期轮换线程
    std::atomic<bool> cycleRotationThreadRunFlag{false};
    std::thread cycleRotationThread;

    void cycleRotationWorker() const {
        while (cycleRotationThreadRunFlag.load()) {
            // 根据轮换时间间隔，实现周期轮换
            const time_t t = bloomFilterEngine->getBLOOM_FILTER_ROTATION_TIME();
            std::this_thread::sleep_for(std::chrono::seconds(t));
            if (cycleRotationThreadRunFlag.load()) {
                // 调用 BloomFilterEngine 的方法进行周期轮换
                bloomFilterEngine->rotate_filters();
                std::cout << "Cycle Rotation! time:" << time(nullptr) << std::endl;
            }
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
            data["max_jwt_life_time"] = std::to_string(bloomFilterEngine->getMAX_JWT_LIFETIME()); // T^max_i
            data["bloom_filter_rotation_time"] = std::to_string(bloomFilterEngine->getBLOOM_FILTER_ROTATION_TIME());
            // T^w_i
            data["bloom_filter_num"] = std::to_string(bloomFilterEngine->getBLOOM_FILTER_NUM()); // n^bf_i
            data["filters_msg_num"] = vectorToString(bloomFilterEngine->getFILTERS_MSG_NUM()); // n^jwt_(i-1,j)
            data["bloom_filter_size"] = std::to_string(bloomFilterEngine->getBLOOM_FILTER_SIZE()); // m^bf_i
            data["hash_function_num"] = std::to_string(bloomFilterEngine->getHASH_FUNCTION_NUM()); // k^hash_i

            const std::string msg = doMsgAssembly(event, data);
            masterServerMsgBridge->sendMsg(msg.c_str());
        }
    }
};


#endif // BLOOM_FILTER_MANAGER_HPP
