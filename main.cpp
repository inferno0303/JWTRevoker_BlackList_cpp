#include <iostream>
#include "src/Controller/Controller.h"

int main() {
    CONTROLLER::Controller sub_thread = CONTROLLER::Controller();
    sub_thread.start();

    // 主线程可以继续做其他事情
    std::cout << "Main thread is doing other work..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10)); // 主线程的模拟工作

    // 停止工作线程
    sub_thread.stop();

    std::cout << "Main thread finished." << std::endl;
    return 0;
}
