#ifndef BLOOM_FILTER_MANAGER_HPP
#define BLOOM_FILTER_MANAGER_HPP

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>
#include <mutex>
#include <condition_variable>
#include "../Network/NetworkUtils/NioTcpMsgSenderReceiver.hpp"

class BloomFilterManager {
public:
    explicit BloomFilterManager(NioTcpMsgSenderReceiver* _nioTcpMsg) : nioTcpMsg(_nioTcpMsg) {
        // 向服务器查询布隆过滤器默认配置

        // 初始化布隆过滤器

        // 开始处理消息线程

        // 开始定时上报状态线程

        // 开始执行周期轮换线程
    }

    ~BloomFilterManager() = default;

private:
    NioTcpMsgSenderReceiver* nioTcpMsg;

    // 定时上报状态线程
    std::atomic<bool> reportStatusRunFlag{false};
    std::thread reportStatusThread;
    void reportStatusWorker() const;

    // 处理消息线程
    std::atomic<bool> processMsgRunFlag{false};
    std::thread processMsgThread;
    void processMsgWorker();

    // 周期轮换定时任务线程
    std::atomic<bool> cycleRotationRunFlag{false};
    std::thread cycleRotationThread;
    void cycleRotationWorker() const;
};

#endif // BLOOM_FILTER_MANAGER_HPP
