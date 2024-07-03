#include "CycleRotationTimerTask.h"

CYCLE_ROTATION_TIMER_TASK::CycleRotationTimerTask::CycleRotationTimerTask(BFS::BloomFilters bloom_filters) : bloom_filters(std::move(bloom_filters)) {
    std::cout << "Starting Cycle Rotation Timer Task..." << std::endl;
}

CYCLE_ROTATION_TIMER_TASK::CycleRotationTimerTask::~CycleRotationTimerTask() {
    stop();
}

void CYCLE_ROTATION_TIMER_TASK::CycleRotationTimerTask::start() {
    if (!workerThread.joinable()) {
        stopFlag.store(false); // 确保每次启动时 stopFlag 都是 false
        workerThread = std::thread(&CYCLE_ROTATION_TIMER_TASK::CycleRotationTimerTask::worker, this);
    }
}

void CYCLE_ROTATION_TIMER_TASK::CycleRotationTimerTask::stop() {
    stopFlag.store(true);
    if (workerThread.joinable()) {
        workerThread.join();
    }
}

void CYCLE_ROTATION_TIMER_TASK::CycleRotationTimerTask::worker() {
    while (!stopFlag.load()) {

        // 根据轮换时间间隔，实现轮换
        time_t t = bloom_filters.getBLOOM_FILTER_ROTATION_TIME();
        std::this_thread::sleep_for(std::chrono::seconds(t));
        if (!stopFlag.load()) {
            // 调用 BloomFilterManager 实现轮换
            bloom_filters.rotate_filters();
            std::cout << "Cycle Rotation! time:" << time(nullptr) << std::endl;
        }
    }
}

