#ifndef JWTREVOKER_BLACKLIST_CYCLE_ROTATION_TIMER_TASK_H
#define JWTREVOKER_BLACKLIST_CYCLE_ROTATION_TIMER_TASK_H

#include <thread>
#include <atomic>
#include "../BloomFIlters/BloomFilters.h"

namespace CYCLE_ROTATION_TIMER_TASK {

    class CycleRotationTimerTask {
    private:
        // 轮换定时任务线程
        std::atomic<bool> stopFlag{true};
        std::thread workerThread;
        void worker();

        // Bloom Filter Manager
        BFS::BloomFilters bloom_filters;

    public:
        explicit CycleRotationTimerTask(BFS::BloomFilters bloom_filters);

        // 启动工作线程
        void start();

        // 停止工作线程
        void stop();

        ~CycleRotationTimerTask();
    };
} // namespace CRTT

#endif //JWTREVOKER_BLACKLIST_CYCLE_ROTATION_TIMER_TASK_H
