#include "BloomFilterScheduler.h"

BloomFilterScheduler::BloomFilterScheduler() {
    // 布隆过滤器初始化，分配内存
    try {
        std::cout << "Allocation bloom filter memory..." << std::endl;
        // 使用 std::make_shared 创建智能指针
        this->bloomFilterEngine = std::make_shared<BloomFilterEngine>(3600 * 24, 1, 8 * 1024, 5);
        std::cout << "Bloom filter memory allocation success!" << std::endl;
    } catch (const std::bad_alloc &e) {
        // 捕获内存分配失败异常
        std::cerr << "Memory allocation failed: " << e.what() << std::endl;
    } catch (const std::exception &e) {
        // 捕获其他异常
        std::cerr << "An exception occurred: " << e.what() << std::endl;
    }

    // 连接到master服务器
    try {
        SOCKET s = connectToServer(SERVER_IP, SERVER_PORT);
        if (s == INVALID_SOCKET) {
            throw std::runtime_error("Auth Error！");
        }
        this->sock = s;
    } catch (const std::exception &e) {
        std::cerr << "An exception occurred: " << e.what() << std::endl;
    }

    // 开始执行节点信息上报模块任务线程
    startReportStatusThread();

    // 开始执行周期轮换线程
    startCycleRotationThread();
}

BloomFilterScheduler::~BloomFilterScheduler() {
    stopCycleRotationThread();
    stopReportStatusThread();
}

std::shared_ptr<BloomFilterEngine> BloomFilterScheduler::getBloomFilterEngine() {
    return bloomFilterEngine;
}

void BloomFilterScheduler::cycleRotationWorker() {
    while (cycleRotationRunFlag.load()) {
        // 根据轮换时间间隔，实现周期轮换
        const time_t t = bloomFilterEngine->getBLOOM_FILTER_ROTATION_TIME();
        std::this_thread::sleep_for(std::chrono::seconds(t));
        if (cycleRotationRunFlag.load()) {
            // 调用 BloomFilterEngine 的方法进行周期轮换
            bloomFilterEngine->rotate_filters();
            std::cout << "Cycle Rotation! time:" << time(nullptr) << std::endl;
        }
    }
}

void BloomFilterScheduler::startCycleRotationThread() {
    if (!cycleRotationThread.joinable()) {
        cycleRotationRunFlag.store(true);
        cycleRotationThread = std::thread(&BloomFilterScheduler::cycleRotationWorker, this);
    }
}

void BloomFilterScheduler::stopCycleRotationThread() {
    cycleRotationRunFlag.store(false);
    if (cycleRotationThread.joinable()) {
        // 等待线程结束
        cycleRotationThread.join();
    }
}

SOCKET BloomFilterScheduler::connectToServer(const char *server_ip, const unsigned short server_port) {
    WSADATA wsaData;
    auto sock = INVALID_SOCKET;
    struct sockaddr_in server_addr{};
    char buffer[BUFFER_SIZE] = {0};

    // 初始化WinSock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("WSAStartup failed: " + std::to_string(WSAGetLastError()));
    }

    // 创建套接字
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        int errorCode = WSAGetLastError();
        WSACleanup();
        throw std::runtime_error("Socket creation error: " + std::to_string(errorCode));
    }

    // 设置目标地址
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    // 将IPv4地址转换成二进制形式
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        closesocket(sock);
        WSACleanup();
        throw std::runtime_error("Invalid address/ Address not supported");
    }

    // 连接到服务端
    if (connect(sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        int errorCode = WSAGetLastError();
        closesocket(sock);
        WSACleanup();
        throw std::runtime_error("Connection Failed: " + std::to_string(errorCode));
    }

    // 发送认证信息
    const char *hello_msg = "hello from client";
    send(sock, hello_msg, strlen(hello_msg), 0);
    std::cout << "Message sent: " << hello_msg << std::endl;

    // 接收认证信息
    int recvLen = recv(sock, buffer, BUFFER_SIZE, 0);
    if (recvLen > 0) {
        buffer[recvLen] = '\0';
        std::cout << "Server response: " << buffer << std::endl;
        return sock;
    } else {
        closesocket(sock);
        return INVALID_SOCKET;
    }
}

void BloomFilterScheduler::reportStatusWorker() {
    // 定时发送节点状态信息
    while (reportStatusRunFlag) {
        // 收集 BloomFilterEngine 信息


        // 发送
        const char *status_msg = R"({"event": "node_status_report", "data": "ok"})";
        send(sock, status_msg, strlen(status_msg), 0);
        std::cout << "[node report] Status sent: " << status_msg << std::endl;

        // 等待
        std::this_thread::sleep_for(std::chrono::seconds(INTERVAL));
    }
}

void BloomFilterScheduler::startReportStatusThread() {
    if (!reportStatusThread.joinable()) {
        reportStatusRunFlag.store(true);
        reportStatusThread = std::thread(&BloomFilterScheduler::reportStatusWorker, this);
    }
}

void BloomFilterScheduler::stopReportStatusThread() {
    reportStatusRunFlag.store(false);
    if (reportStatusThread.joinable()) {
        // 等待线程结束
        reportStatusThread.join();
    }
}

void BloomFilterScheduler::startReceiveThread() {
    if (!receiveThread.joinable()) {
        receiveRunFlag.store(true);
        receiveThread = std::thread(&BloomFilterScheduler::receiveWorker, this);
    }
}

void BloomFilterScheduler::stopReceiveThread() {
    receiveRunFlag.store(false);
    if (receiveThread.joinable()) {
        // 等待线程结束
        receiveThread.join();
    }
}

void BloomFilterScheduler::receiveWorker() {
    char buffer[BUFFER_SIZE];
    while (receiveRunFlag) {
        // 接收
        int recvLen = recv(sock, buffer, BUFFER_SIZE, 0);
        if (recvLen > 0) {
            buffer[recvLen] = '\0';
            std::cout << "[server response] server response: " << buffer << std::endl;

            // 检查是否为命令
            if (strstr(buffer, R"({"event": "cmd")") != nullptr) {
                std::lock_guard<std::mutex> lock(cmdMutex);
                receivedCmd.assign(buffer);
                cmdCv.notify_one();
            }
        }
    }
}

void BloomFilterScheduler::startProcessCommandsThread() {
    if (!processCommandsThread.joinable()) {
        processCommandsRunFlag.store(true);
        processCommandsThread = std::thread(&BloomFilterScheduler::processCommandsWorker, this);
    }
}

void BloomFilterScheduler::stopProcessCommandsThread() {
    processCommandsRunFlag.store(false);
    if (processCommandsThread.joinable()) {
        // 等待线程结束
        processCommandsThread.join();
    }
}

void BloomFilterScheduler::processCommandsWorker() {
    while (processCommandsRunFlag) {
        std::unique_lock<std::mutex> lock(cmdMutex);
        cmdCv.wait(lock, [this] { return !this->receivedCmd.empty(); });

        // 处理接收到的命令
        std::cout << "[command processor] Processing command: " << receivedCmd << std::endl;

        // 处理完毕，清空命令
        receivedCmd.clear();
    }
}

std::string BloomFilterScheduler::collectBloomFilterEngineInfo() {
    const char *status_msg = R"({"event": "node_status_report", "data": "ok"})";
    return std::string("hello world");
}
