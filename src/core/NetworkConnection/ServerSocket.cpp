#include "ServerSocket.h"

SERVER_SOCKET::ServerSocket::ServerSocket(unsigned short port) {
    // 设置端口号
    if (port < 80) {
        throw std::invalid_argument("The port number cannot be less than 80.");
    }
    this->PORT = port;

    // 初始化WinSock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        int errorCode = WSAGetLastError();
        throw std::runtime_error("WSAStartup failed: " + std::to_string(errorCode));
    }

    // 创建套接字
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        int errorCode = WSAGetLastError();
        WSACleanup();
        throw std::runtime_error("Socket creation failed: " + std::to_string(errorCode));
    }

    // 设置服务器地址结构
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    // 绑定套接字到端口
    if (bind(serverSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        int errorCode = WSAGetLastError();
        closesocket(serverSocket);
        WSACleanup();
        throw std::runtime_error("Bind failed: " + std::to_string(errorCode));
    }

    // 监听端口
    if (listen(serverSocket, 3) == SOCKET_ERROR) {
        int errorCode = WSAGetLastError();
        closesocket(serverSocket);
        WSACleanup();
        throw std::runtime_error("Listen failed: " + std::to_string(errorCode));
    }

    std::cout << "Waiting for incoming connections on port " << PORT << "..." << std::endl;
}

SERVER_SOCKET::ServerSocket::~ServerSocket() {
    stopServerListenThread();
}

void SERVER_SOCKET::ServerSocket::startServerListenThread() {
    // 如果线程是停止的
    if (!serverListenThread.joinable()) {
        stopServerListenFlag.store(false); // 将停止信号设置为 false
        serverListenThread = std::thread(&SERVER_SOCKET::ServerSocket::serverListenWorker, this);
    }
}

void SERVER_SOCKET::ServerSocket::stopServerListenThread() {
    // 将停止信号设置为 true
    stopServerListenFlag.store(true);
    if (serverListenThread.joinable()) {
        serverListenThread.join();
    }
}


void SERVER_SOCKET::ServerSocket::serverListenWorker() {
    // 主循环，接受客户端连接
    int clientAddrSize = sizeof(clientAddr);
    while (!stopServerListenFlag.load()) {
        if ((clientSocket = accept(serverSocket, (struct sockaddr *) &clientAddr, &clientAddrSize)) == INVALID_SOCKET) {
            int errorCode = WSAGetLastError();
            closesocket(serverSocket);
            WSACleanup();
            throw std::runtime_error("Accept failed: " + std::to_string(errorCode));
        }

        std::cout << "NetworkConnection accepted." << std::endl;

        // 创建一个新的线程来处理客户端连接
        std::thread clientThread(&ServerSocket::clientSocketWorker, this, clientSocket);
        clientThread.detach(); // 使线程在后台运行
    }
}

void SERVER_SOCKET::ServerSocket::clientSocketWorker(SOCKET clientSocket_) const {
    char buffer[BUFFER_SIZE] = {};
    int recvLen;

    // 接收数据
    while ((recvLen = recv(clientSocket_, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[recvLen] = '\0'; // 添加字符串结束符
        std::cout << "Thread ID " << std::this_thread::get_id() << " received: " << buffer << std::endl;
    }

    // 异常处理
    if (recvLen == 0) {
        // 客户端关闭连接
        std::cout << "Client disconnected" << std::endl;
    } else if (recvLen == SOCKET_ERROR) {
        // 网络中断或其他错误
        std::cerr << "Recv failed: " << WSAGetLastError() << std::endl;
    }

    // 关闭客户端套接字
    closesocket(clientSocket);

    // 终止线程并清理
}