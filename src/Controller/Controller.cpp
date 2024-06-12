#include "Controller.h"

CONTROLLER::Controller::Controller() : stopFlag(false) {}

void CONTROLLER::Controller::start()
{
    if (!workerThread.joinable())
    {
        stopFlag.store(false); // 确保每次启动时 stopFlag 都是 false
        workerThread = std::thread(&Controller::worker, this);
    }
}

void CONTROLLER::Controller::stop()
{
    stopFlag.store(true);
    if (workerThread.joinable())
    {
        workerThread.join();
    }
}

void CONTROLLER::Controller::worker()
{
    while (!stopFlag.load())
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!stopFlag.load())
        {
            std::cout << "Periodic task executed." << std::endl;
        }
    }
}

CONTROLLER::Controller::~Controller()
{
    stop();
}