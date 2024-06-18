#ifndef JWTREVOKER_BLACKLIST_CYCLE_ROTATION_TIMER_TASK_H
#define JWTREVOKER_BLACKLIST_CYCLE_ROTATION_TIMER_TASK_H

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include "../BloomFilterManager/BloomFilterManager.h"

namespace CYCLE_ROTATION_TIMER_TASK {

    class CycleRotationTimerTask {
    private:
        std::atomic<bool> stopFlag;
        std::thread workerThread;

        // 线程任务
        void worker();

        // Bloom Filter Manager
        BFM::BloomFilterManager bloomFilterManager;

    public:
        explicit CycleRotationTimerTask(BFM::BloomFilterManager bfm);

        // 启动工作线程
        void start();

        // 停止工作线程
        void stop();

        ~CycleRotationTimerTask();
    };
} // namespace CRTT

#endif //JWTREVOKER_BLACKLIST_CYCLE_ROTATION_TIMER_TASK_H
