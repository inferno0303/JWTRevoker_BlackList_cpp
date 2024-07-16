#ifndef BLOOMFILTER_SCHEDULER_H
#define BLOOMFILTER_SCHEDULER_H

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "../BloomFilterEngine/BloomFilterEngine.h"

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9999
#define BUFFER_SIZE 1024
#define INTERVAL 5 // 时间间隔，单位：秒

class BloomFilterScheduler {
private:
    // BloomFilterEngine 资源共享智能指针
    std::shared_ptr<BloomFilterEngine> bloomFilterEngine;

    // 周期轮换定时任务线程
    std::atomic<bool> cycleRotationRunFlag{false};
    std::thread cycleRotationThread;
    void cycleRotationWorker();
    void startCycleRotationThread();
    void stopCycleRotationThread();

    // 连接master服务器
    static SOCKET connectToServer(const char *, unsigned short);
    SOCKET sock{};

    // 上报状态线程
    std::atomic<bool> reportStatusRunFlag{false};
    std::thread reportStatusThread;
    void reportStatusWorker();
    void startReportStatusThread();
    void stopReportStatusThread();

    // 收集信息函数
    std::string collectBloomFilterEngineInfo();

    // 接收信息线程
    std::atomic<bool> receiveRunFlag{false};
    std::thread receiveThread;
    void receiveWorker();
    void startReceiveThread();
    void stopReceiveThread();

    // 接收信息线程 与 处理命令线程 线程间同步互斥锁、条件变量
    std::mutex cmdMutex;
    std::condition_variable cmdCv;

    // 接收到的命令
    std::string receivedCmd;

    // 处理命令线程
    std::atomic<bool> processCommandsRunFlag{false};
    std::thread processCommandsThread;
    void processCommandsWorker();
    void startProcessCommandsThread();
    void stopProcessCommandsThread();


public:
    BloomFilterScheduler();

    ~BloomFilterScheduler();

    // 允许其他对象访问 BloomFilterEngine 共享指针
    std::shared_ptr<BloomFilterEngine> getBloomFilterEngine();

};

#endif // BLOOMFILTER_SCHEDULER_H
