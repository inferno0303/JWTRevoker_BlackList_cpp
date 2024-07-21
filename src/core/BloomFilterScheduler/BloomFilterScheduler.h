#ifndef BLOOMFILTER_SCHEDULER_H
#define BLOOMFILTER_SCHEDULER_H

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "../BloomFilterEngine/BloomFilterEngine.h"

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9999
#define BUFFER_SIZE 65535
#define INTERVAL 10 // 时间间隔，单位：秒

class BloomFilterScheduler {
private:
    // BloomFilterEngine 资源共享智能指针
    std::shared_ptr<BloomFilterEngine> bloomFilterEngine;

    // 连接master服务器
    static SOCKET connectToServer(const char *, unsigned short);
    SOCKET sock{};

    // 上报状态线程
    std::atomic<bool> reportStatusRunFlag{false};
    std::thread reportStatusThread;
    void reportStatusWorker() const;
    void startReportStatusThread();
    void stopReportStatusThread();

    // 接收事件消息线程
    std::atomic<bool> receiveRunFlag{false};
    std::thread receiveThread;
    void receiveWorker();
    void startReceiveThread();
    void stopReceiveThread();

    // 接收到的命令
    std::queue<const char*> receivedEventQueue;

    // 接收信息线程 与 处理命令线程 线程间同步互斥锁、条件变量
    std::mutex receivedEventMutex;
    std::condition_variable receivedEventCv;

    // 处理命令线程
    std::atomic<bool> processCommandsRunFlag{false};
    std::thread processCommandsThread;
    void processCommandsWorker();
    void startProcessCommandsThread();
    void stopProcessCommandsThread();

    // 周期轮换定时任务线程
    std::atomic<bool> cycleRotationRunFlag{false};
    std::thread cycleRotationThread;
    void cycleRotationWorker() const;
    void startCycleRotationThread();
    void stopCycleRotationThread();


public:
    BloomFilterScheduler();

    ~BloomFilterScheduler();

    // 允许其他对象访问 BloomFilterEngine 共享指针
    // std::shared_ptr<BloomFilterEngine> getBloomFilterEngine();

};

#endif // BLOOMFILTER_SCHEDULER_H
