#ifndef BLOOM_FILTER_MANAGER_HPP
#define BLOOM_FILTER_MANAGER_HPP

#include <iostream>
#include <thread>
#include <atomic>
#include <memory>

#include "../Network/NetworkUtils/MsgFormat.hpp"
#include "../Network/NetworkUtils/NioTcpMsgBridge.hpp"
#include "../BloomFilterEngine/BloomFilterEngine.h"
#include "../Utils/StringConverter.hpp"

class BloomFilterManager {
public:
    explicit BloomFilterManager(NioTcpMsgBridge* nio_) : nioTcpMsgBridge(nio_) {
        // 启动处理消息线程
        if (!processMsgThread.joinable()) {
            processMsgThreadRunFlag.store(true);
            processMsgThread = std::thread(&BloomFilterManager::processMsgWorker, this);
        }

        // 向服务器查询布隆过滤器默认配置
        this->getBFDefaultConfig();

        // 开始定时上报状态线程

    }

    ~BloomFilterManager() {
        // 停止处理消息线程
        processMsgThreadRunFlag.store(false);
        if (processMsgThread.joinable()) {
            processMsgThread.join();
        }
    };

private:
    NioTcpMsgBridge* nioTcpMsgBridge{};

    std::shared_ptr<BloomFilterEngine> bloomFilterEngine{};

    // 处理消息线程
    std::atomic<bool> processMsgThreadRunFlag{false};
    std::thread processMsgThread;

    void processMsgWorker() {
        while (processMsgThreadRunFlag) {
            const char* msg = nioTcpMsgBridge->recvMsg();
            std::cout << msg << std::endl;
            std::string event;
            std::map<std::string, std::string> data;
            doMsgParse(std::string(msg), event, data);
            delete[] msg;

            if (event == "auth_success") {
                std::cout << "auth_success!" << std::endl;
            }

            if (event == "ping_from_server") {
                replyPongFromClient();
            }

            if (event == "bloom_filter_default_config") {
                initBFByDefaultConfig(data);
            }

            if (event == "get_node_status") {
                replyNodeStatus();
            }

        }
    }

    // 发送 get_bloom_filter_default_config 消息，查询布隆过滤器默认配置
    void getBFDefaultConfig() const {
        const std::string event = "get_bloom_filter_default_config";
        std::map<std::string, std::string> data;
        data["client_uid"] = "0001";
        const std::string msg = doMsgAssembly(event, data);
        nioTcpMsgBridge->sendMsg(msg.c_str());
    };

    // 发送 pong_from_client 消息
    void replyPongFromClient() const {
        const std::string event = "pong_from_client";
        std::map<std::string, std::string> data;
        data["client_uid"] = "0001";
        const std::string msg = doMsgAssembly(event, data);
        nioTcpMsgBridge->sendMsg(msg.c_str());
    }

    // 根据默认配置初始化布隆过滤器
    void initBFByDefaultConfig(std::map<std::string, std::string> data) {
        const std::string maxJwtLifeTime = data["max_jwt_life_time"];
        const std::string bloomFilterRotationTime = data["bloom_filter_rotation_time"];
        const std::string bloomFilterSize = data["bloom_filter_size"];
        const std::string numHashFunction = data["num_hash_function"];

        // 布隆过滤器初始化，分配内存
        try {
            std::cout << "Allocation bloom filter memory..." << std::endl;
            // 使用 std::make_shared 创建智能指针
            this->bloomFilterEngine = std::make_shared<BloomFilterEngine>(
                stringToUInt(maxJwtLifeTime),
                stringToUInt(bloomFilterRotationTime),
                stringToSizeT(bloomFilterSize),
                stringToUInt(numHashFunction));
            std::cout << "Bloom filter memory allocation success!" << std::endl;
        } catch (const std::bad_alloc& e) {
            // 捕获内存分配失败异常
            std::cerr << "Memory allocation failed: " << e.what() << std::endl;
            throw std::runtime_error("Memory allocation failed!");
        } catch (const std::exception& e) {
            // 捕获其他异常
            std::cerr << "An exception occurred: " << e.what() << std::endl;
            throw std::runtime_error("Memory allocation failed!");
        }

        // 启动周期轮换线程


    }

    // 发送 node_status 消息
    void replyNodeStatus() const {
        const std::string event = "node_status";
        std::map<std::string, std::string> data;

        data["client_uid"] = "0001";
        data["max_jwt_life_time"] = std::to_string(bloomFilterEngine->getMAX_JWT_LIFETIME()); // T^max_i
        data["bloom_filter_rotation_time"] = std::to_string(bloomFilterEngine->getBLOOM_FILTER_ROTATION_TIME()); // T^w_i
        data["bloom_filter_num"] = std::to_string(bloomFilterEngine->getBLOOM_FILTER_NUM()); // n^bf_i
        data["filters_msg_num"] = vectorToString(bloomFilterEngine->getFILTERS_MSG_NUM()); // n^jwt_(i-1,j)
        data["bloom_filter_size"] = std::to_string(bloomFilterEngine->getBLOOM_FILTER_SIZE()); // m^bf_i
        data["hash_function_num"] = std::to_string(bloomFilterEngine->getHASH_FUNCTION_NUM()); // k^hash_i

        const std::string msg = doMsgAssembly(event, data);
        std::cout << msg << std::endl;
        nioTcpMsgBridge->sendMsg(msg.c_str());
    }

    // 周期轮换定时任务线程
    std::atomic<bool> cycleRotationRunFlag{false};
    std::thread cycleRotationThread;
    void cycleRotationWorker() const;
};


#endif // BLOOM_FILTER_MANAGER_HPP
