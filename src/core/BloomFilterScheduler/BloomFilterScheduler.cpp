#include "BloomFilterScheduler.h"
#include "../Utils/nlohmann/json.hpp"

BloomFilterScheduler::BloomFilterScheduler() {

    // 连接到master服务器，将套接字保存到成员变量sock中
    try {
        const SOCKET s = connectToServer(SERVER_IP, SERVER_PORT);
        if (s == INVALID_SOCKET) {
            throw std::runtime_error("Connect to server error!");
        }
        this->sock = s;
    } catch (const std::exception &e) {
        std::cerr << "An exception occurred: " << e.what() << std::endl;
        throw std::runtime_error("Connect to server error!");
    }

    // 布隆过滤器初始化，分配内存
    try {
        std::cout << "Allocation bloom filter memory..." << std::endl;
        // 使用 std::make_shared 创建智能指针
        this->bloomFilterEngine = std::make_shared<BloomFilterEngine>(3600 * 24, 3600, 8 * 1024, 5);
        std::cout << "Bloom filter memory allocation success!" << std::endl;
    } catch (const std::bad_alloc &e) {
        // 捕获内存分配失败异常
        std::cerr << "Memory allocation failed: " << e.what() << std::endl;
        throw std::runtime_error("Memory allocation failed!");
    } catch (const std::exception &e) {
        // 捕获其他异常
        std::cerr << "An exception occurred: " << e.what() << std::endl;
        throw std::runtime_error("Memory allocation failed!");
    }

    // 开始执行节点信息上报模块任务线程
    // startReportStatusThread();

    // 开始执行接收信息线程
    startReceiveThread();

    // 开始执行处理命令线程
    startProcessCommandsThread();

    // 开始执行周期轮换线程
    startCycleRotationThread();
}

BloomFilterScheduler::~BloomFilterScheduler() {
    stopReportStatusThread();
    stopReceiveThread();
    stopProcessCommandsThread();
    stopCycleRotationThread();
}

// 连接到服务器，包含认证
SOCKET BloomFilterScheduler::connectToServer(const char *server_ip, const unsigned short server_port) {
    WSADATA wsaData;
    auto sock = INVALID_SOCKET;
    sockaddr_in server_addr{};
    char buffer[BUFFER_SIZE] = {0};

    // 初始化WinSock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("WSAStartup failed: " + std::to_string(WSAGetLastError()));
    }

    // 创建套接字
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        const int errorCode = WSAGetLastError();
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
    if (connect(sock, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) < 0) {
        const int errorCode = WSAGetLastError();
        closesocket(sock);
        WSACleanup();
        throw std::runtime_error("Connection Failed: " + std::to_string(errorCode));
    }

    // 发送认证信息
    nlohmann::json _data;
    _data["uid"] = "0001";
    _data["token"] = "abcde";
    const std::string hello_msg = R"({"event":"hello_from_client","data":)" + _data.dump() + "}";
    send(sock, hello_msg.c_str(), static_cast<int>(strlen(hello_msg.c_str())), 0);
    std::cout << "Client event: " << hello_msg.c_str() << std::endl;

    // 接收认证信息
    const int recvLen = recv(sock, buffer, BUFFER_SIZE, 0);
    if (recvLen > 0) {
        buffer[recvLen] = '\0';
        std::cout << "Server event: " << buffer << std::endl;
        return sock;
    }
    closesocket(sock);
    return INVALID_SOCKET;
}

void BloomFilterScheduler::reportStatusWorker() const {
    // 定时发送节点状态信息
    while (reportStatusRunFlag) {

        // 收集节点状态信息
        nlohmann::json _data;
        _data["uid"] = "0001";
        _data["MAX_JWT_LIFETIME"] = bloomFilterEngine->getMAX_JWT_LIFETIME(); // T^max_i
        _data["BLOOM_FILTER_ROTATION_TIME"] = bloomFilterEngine->getBLOOM_FILTER_ROTATION_TIME(); // T^w_i
        _data["NUM_BLOOM_FILTER"] = bloomFilterEngine->getNUM_BLOOM_FILTER(); // n^bf_i
        _data["FILTERS_NUM_MSG"] = bloomFilterEngine->getFILTERS_NUM_MSG(); // n^jwt_(i-1,j)
        _data["BLOOM_FILTER_SIZE"] = bloomFilterEngine->getBLOOM_FILTER_SIZE(); // m^bf_i
        _data["NUM_HASH_FUNCTION"] = bloomFilterEngine->getNUM_HASH_FUNCTION(); // k^hash_i
        std::string msg = R"({"event":"node_status_report","data":)" + _data.dump() + "}";

        // 发送数据
        if (sock != INVALID_SOCKET) {
            send(sock, msg.c_str(), static_cast<int>(strlen(msg.c_str())), 0);
            // std::cout << "[node_status] sent: " << msg.c_str() << std::endl;
        }

        // 等待一段时间
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
        const int recvLen = recv(sock, buffer, BUFFER_SIZE, 0);
        if (recvLen > 0) {
            buffer[recvLen] = '\0';
            std::cout << "[Server event] " << buffer << std::endl;

            // 解析JSON字符串
            try {
                nlohmann::json jsonObject = nlohmann::json::parse(buffer);

                // 如果接收到 event 关键字
                std::string event = jsonObject["event"];
                if (!event.empty()) {
                    // std::lock_guard<std::mutex> lock(cmdMutex);
                    // receivedCmd.assign(buffer);
                    //
                    // // 唤醒处理命令线程
                    // cmdCv.notify_one();
                }
            } catch (nlohmann::json::parse_error& e) {
                std::cerr << "JSON parse error: " << e.what() << std::endl;
            } catch (std::exception& e) {
                std::cerr << "Runtime exception: " << e.what() << std::endl;
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

        try {
            // 解析JSON字符串
            nlohmann::json jsonObject = nlohmann::json::parse(receivedCmd);
            std::string event = jsonObject["event"];
            std::string data = jsonObject["data"];

            // 如果接收到 ping_from_server 事件
            if (event == "ping_from_server") {
                nlohmann::json dataObject = nlohmann::json::parse(data);
                std::string client_uid = dataObject["client_uid"];
                if (client_uid == "0001") {
                    // 发送 pong_from_client 消息
                    nlohmann::json _data;
                    _data["uid"] = "0001";
                    std::string msg = R"({"event":"pong_from_client","data":)" + _data.dump() + "}";
                    if (sock != INVALID_SOCKET) {
                        send(sock, msg.c_str(), static_cast<int>(strlen(msg.c_str())), 0);
                    }
                }
            }

            // 如果接收到 其他 事件

        } catch (nlohmann::json::parse_error& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
        } catch (std::exception& e) {
            std::cerr << "Runtime exception2: " << e.what() << std::endl;
        }

        // 处理完毕，清空
        receivedCmd.clear();
    }
}

void BloomFilterScheduler::cycleRotationWorker() const {
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
