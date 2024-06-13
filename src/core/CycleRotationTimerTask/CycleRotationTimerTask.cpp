#include "CycleRotationTimerTask.h"

#include <utility>
#include "../BloomFilterManager/BloomFilterManager.h"

CRTT::CycleRotationTimerTask::CycleRotationTimerTask(BFM::BloomFilterManager bfm) : bloomFilterManager(std::move(bfm)),
                                                                                    stopFlag(false) {}

CRTT::CycleRotationTimerTask::~CycleRotationTimerTask() {
    stop();
}

void CRTT::CycleRotationTimerTask::start() {
    if (!workerThread.joinable()) {
        stopFlag.store(false); // 确保每次启动时 stopFlag 都是 false
        workerThread = std::thread(&CRTT::CycleRotationTimerTask::worker, this);
    }
}

void CRTT::CycleRotationTimerTask::stop() {
    stopFlag.store(true);
    if (workerThread.joinable()) {
        workerThread.join();
    }
}

void CRTT::CycleRotationTimerTask::worker() {
    while (!stopFlag.load()) {

        // 根据轮换时间间隔，实现轮换
        time_t t = bloomFilterManager.getBLOOM_FILTER_ROTATION_TIME();
        std::this_thread::sleep_for(std::chrono::seconds(t));
        if (!stopFlag.load()) {
            // 调用 BloomFilterManager 实现轮换
            bloomFilterManager.rotate_filters();
            std::cout << "Cycle Rotation!" << std::endl;
        }
    }
}

