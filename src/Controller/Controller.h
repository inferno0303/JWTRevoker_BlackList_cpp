// Controller.h
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

namespace CONTROLLER
{

    class Controller
    {
    public:
        Controller();

        // 启动工作线程
        void start();

        // 停止工作线程
        void stop();

        ~Controller();

    private:
        std::atomic<bool> stopFlag;
        std::thread workerThread;
        void worker();
    };

} // namespace CONTROLLER

#endif // CONTROLLER_H
