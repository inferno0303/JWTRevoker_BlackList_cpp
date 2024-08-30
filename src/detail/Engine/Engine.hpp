#ifndef BLACK_LIST_ENGINE_HPP
#define BLACK_LIST_ENGINE_HPP

#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <chrono>
#include <atomic>
#include <thread>
#include <set>
#include <filesystem>
#include "BaseBloomFilter.hpp"
#include "../Utils/ConfigReader.hpp"
#include "../Utils/ThreadSafeQueue.hpp"


inline std::vector<BaseBloomFilter> getNewFilters(const unsigned int &filtersNum,
                                                  const unsigned int &bloomFilterSize,
                                                  const unsigned int &hashFunctionNum) {
    std::vector<BaseBloomFilter> newFilters;
    newFilters.reserve(filtersNum);
    for (unsigned int i = 0; i < filtersNum; ++i) { newFilters.emplace_back(bloomFilterSize, hashFunctionNum); }
    return newFilters;
}

inline void printLogo(const float totalSize) {
    const auto logo = R"(
          ____  _                         ______ _ _ _
         |  _ \| |                       |  ____(_) | |
         | |_) | | ___   ___  _ __ ___   | |__   _| | |_ ___ _ __ ___
         |  _ <| |/ _ \ / _ \| '_ ` _ \  |  __| | | | __/ _ \ '__/ __|
         | |_) | | (_) | (_) | | | | | | | |    | | | ||  __/ |  \__ \
         |____/|_|\___/ \___/|_| |_| |_| |_|    |_|_|\__\___|_|  |___/  )";
    std::cout << logo;
    std::cout << " Memory used: " << totalSize << " MBytes" << std::endl << std::endl;
}


class Engine {
public:
    explicit Engine(const std::map<std::string, std::string> &_config) : config(_config) {
    }

    ~Engine() {
        // 停止周期轮换线程
        rotateFiltersRunFlag.store(false);
        if (rotateFiltersThread.joinable()) { rotateFiltersThread.join(); }

        // 释放布隆过滤器
        filters.clear();

        // 停止日志记录线程
        logRunFlag.store(false);
        if (logThread.joinable()) { logThread.join(); }
    }

    void init(const unsigned int _maxJwtLifeTime, const unsigned int _rotationInterval, const size_t _bloomFilterSize,
              const unsigned int _hashFunctionNum) {
        if (_maxJwtLifeTime == 0) throw std::invalid_argument("maxJwtLifeTime cannot be 0.");
        if (_rotationInterval == 0) throw std::invalid_argument("rotationInterval cannot be 0.");
        if (_bloomFilterSize == 0) throw std::invalid_argument("bloomFilterSize cannot be 0.");
        if (_hashFunctionNum == 0) throw std::invalid_argument("hashFunctionNum cannot be 0.");
        maxJwtLifeTime = _maxJwtLifeTime;
        rotationInterval = _rotationInterval;
        bloomFilterSize = _bloomFilterSize;
        hashFunctionNum = _hashFunctionNum;

        // 计算所需布隆过滤器的个数
        filtersNum = std::ceil(maxJwtLifeTime / rotationInterval);

        std::cout << "[Engine] Initializing bloom filter engine..." << std::endl;

        // 初始化过滤器
        auto _filters = getNewFilters(filtersNum, bloomFilterSize, hashFunctionNum);

        // 从日志中恢复记录到过滤器中
        recoverFromLog(_filters);

        filters.clear();
        filters = _filters;

        // 启动周期轮换线程
        if (!rotateFiltersThread.joinable()) {
            rotateFiltersRunFlag.store(true);
            rotateFiltersThread = std::thread(&Engine::rotateBloomFilterWorker, this);
        }

        // 启动日志记录线程
        if (!logThread.joinable()) {
            logRunFlag.store(true);
            logThread = std::thread(&Engine::logWorker, this);
        }

        printLogo(static_cast<float>(filtersNum * bloomFilterSize) / 8388608);
    }

    // 重建布隆过滤器（布隆过滤器参数改变，需要重建）
    void adjustFiltersParam(const unsigned int _maxJwtLifeTime, const unsigned int _rotationInterval,
                            const size_t _bloomFilterSize, const unsigned int _hashFunctionNum) {
        maxJwtLifeTime = _maxJwtLifeTime;
        rotationInterval = _rotationInterval;
        bloomFilterSize = _bloomFilterSize;
        hashFunctionNum = _hashFunctionNum;

        // 计算所需布隆过滤器的个数
        filtersNum = std::ceil(maxJwtLifeTime / rotationInterval);

        std::cout << "[Engine] Adjust bloom filter engine..." << std::endl;

        // 初始化过滤器
        auto _filters = getNewFilters(filtersNum, bloomFilterSize, hashFunctionNum);

        // 从日志中恢复记录到过滤器中
        recoverFromLog(_filters);

        printLogo(static_cast<float>(filtersNum * bloomFilterSize) / 8388608);

        std::unique_lock lock(filtersMtx);
        filters.clear();
        filters = _filters;
        lock.unlock();
        adjustFiltersCv.notify_all(); // 通知周期轮换线程，重新等待轮换计时
    }

    // 写入布隆过滤器
    void revokeJwt(const std::string &token, const time_t &expTime) {
        // 计算这个 token 还剩多长时间过期
        const auto now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        const time_t remainingTime = expTime - now_c;

        // 防止系统时间错误（系统时间晚于过期时间，导致是负数）
        if (remainingTime <= 0) return;

        // 防止剩余时长超出 maxJwtLifeTime
        if (remainingTime > maxJwtLifeTime) return;

        // 计算需要写入到多少个布隆过滤器中
        const unsigned int num = std::ceil(remainingTime / rotationInterval);
        if (num > filtersNum) return;

        // 分别写入到多个布隆过滤器中
        for (unsigned int i = 0; i < num; ++i) filters[i].add(token);
    }

    // 查询是否在布隆过滤器中
    bool isRevoke(const std::string &token, const time_t &expTime) const {
        // 计算这个 token 还剩多长时间过期
        const auto now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        const time_t remainingTime = expTime - now_c;

        // 防止系统时间错误（系统时间晚于过期时间，导致是负数）
        if (remainingTime <= 0) return false;

        // 防止剩余时长超出 maxJwtLifeTime
        if (remainingTime > maxJwtLifeTime) return false;

        // 计算需要查询多少个布隆过滤器
        const unsigned int num = std::ceil(remainingTime / rotationInterval);
        if (num > filtersNum) return false;

        // 分别查询多个布隆过滤器中
        for (unsigned int i = 0; i < num; ++i) {
            // 如果任意一个布隆过滤器返回不存在，则肯定不存在于黑名单中
            if (!filters[i].contains(token)) { return false; }
        }
        // 如果多个布隆过滤器都返回存在，则可能存在于黑名单中
        return true;
    }

    // 将撤回记录写入日志
    void logRevoke(const std::string &token, const time_t &expTime) {
        logQueue.enqueue(token + "," + std::to_string(expTime));
    }

    // getter方法，用于节点状态上报
    unsigned long getMaxJwtLifeTime() const { return maxJwtLifeTime; }
    unsigned long getRotationInterval() const { return rotationInterval; }
    size_t getBloomFilterSize() const { return bloomFilterSize; }
    unsigned int getHashFunctionNum() const { return hashFunctionNum; }
    unsigned int getBloomFilterNum() const { return filtersNum; }

    std::vector<unsigned long> getBlackListMsgNum() const {
        std::vector<unsigned long> blackListMsgNum;
        blackListMsgNum.reserve(filtersNum);
        for (const auto &baseBloomFilter: filters) { blackListMsgNum.push_back(baseBloomFilter.getMsgNum()); }
        return blackListMsgNum;
    }

private:
    const std::map<std::string, std::string> &config;
    std::vector<BaseBloomFilter> filters; // 布隆过滤器数组
    unsigned long maxJwtLifeTime = 0; // jwt最大生存时长
    unsigned long rotationInterval = 0; // 周期轮换间隔
    size_t bloomFilterSize = 0; // 每个布隆过滤器尺寸
    unsigned int hashFunctionNum = 0; // 哈希函数个数
    unsigned int filtersNum = 0; // 布隆过滤器个数
    std::mutex filtersMtx; // 布隆过滤器读写锁（重建过程中，禁止读写）
    std::condition_variable adjustFiltersCv; // 用于调整布隆过滤器参数后的条件变量（通知周期轮换线程）

    // 持久化线程
    ThreadSafeQueue<std::string> logQueue{}; // 日志队列
    std::atomic<bool> logRunFlag{false};
    std::thread logThread;

    void logWorker() {
        while (logRunFlag.load()) {
            // 从队列中取出消息
            std::string msg = logQueue.dequeue();

            // 计算当前时刻的整点时间戳
            const auto now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::tm *tm = std::localtime(&now_c);
            tm->tm_min = 0; // 将分钟设为0
            tm->tm_sec = 0; // 将秒钟设为0
            const auto hourlyTimestamp = std::mktime(tm);

            // 拼接文件名
            std::filesystem::path filePath = config.at("log_file_path");
            filePath /= std::to_string(hourlyTimestamp) + ".txt"; // 拼接路径

            // 写入文件
            if (std::ofstream logFile(filePath, std::ios::app); logFile.is_open()) {
                logFile << msg << std::endl;
            }
        }
    }

    // 从日志中恢复
    void recoverFromLog(std::vector<BaseBloomFilter> &_filters) const {
        // 计算当前时刻的整点时间戳
        const std::time_t now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm *tm = std::localtime(&now_c);
        tm->tm_min = 0;
        tm->tm_sec = 0;
        std::time_t hourlyTimestamp = std::mktime(tm);

        // 文件路径列表和文件大小列表

        std::vector<std::filesystem::path> foundFiles;
        size_t fileSizes = 0;

        // 循环检查文件是否存在，并获取文件大小
        for (int i = 0; i < 24; ++i) {
            std::filesystem::path filePath = config.at("log_file_path");
            filePath /= std::to_string(hourlyTimestamp) + ".txt"; // 拼接路径
            if (exists(filePath)) {
                foundFiles.push_back(filePath);
                fileSizes += file_size(filePath);
            }
            hourlyTimestamp -= 3600; // 减去1小时
        }

        // 遍历目录中的所有文件
        std::set foundFilesSet(foundFiles.begin(), foundFiles.end());
        for (const auto &entry: std::filesystem::directory_iterator(config.at("log_file_path"))) {
            if (entry.is_regular_file()) {
                // 如果文件不在foundFiles中，删除它
                if (!foundFilesSet.contains(entry.path())) std::filesystem::remove(entry.path());
            }
        }

        // 逐步导入文件内容
        size_t readBytes = 0;
        for (const auto &it: foundFiles) {
            std::ifstream file(it);
            if (!file.is_open()) {
                std::cerr << "[Engine] Error opening file: " << it << std::endl;
                return;
            }
            std::string line;
            while (std::getline(file, line)) {
                std::istringstream iss(line);

                // 解析 token 字符串
                if (std::string token; std::getline(iss, token, ',')) {
                    // 解析 expTime（字符串）
                    if (std::string expTimeStr; std::getline(iss, expTimeStr)) {
                        auto expTime = static_cast<time_t>(std::stol(expTimeStr));

                        // 写入布隆过滤器
                        const time_t remainingTime = expTime - std::chrono::system_clock::to_time_t(
                                                         std::chrono::system_clock::now()); // 计算这个 token 还剩多长时间过期
                        if (remainingTime <= 0) continue;
                        if (remainingTime > maxJwtLifeTime) continue;

                        // 计算需要写入到多少个布隆过滤器中
                        const unsigned int num = std::ceil(remainingTime / rotationInterval);
                        if (num > filtersNum) continue;

                        // 分别写入到多个布隆过滤器中
                        for (unsigned int i = 0; i < num; ++i) _filters[i].add(token);

                        // 显示进度
                        readBytes += 49; // 每行是一条记录，一条记录 49 bytes
                        if (readBytes % 4900000 == 0) {
                            float p = static_cast<float>(readBytes) / static_cast<float>(fileSizes) * 100;
                            std::cout << "[Engine] Recover from log: " << p << "%" << std::endl;
                        }
                    }
                }
            }
            file.close();
        }
        std::cout << "[Engine] Recover from log is done, " << fileSizes / 49 << " items have been loaded." << std::endl;
    }

    // 周期轮换线程
    std::atomic<bool> rotateFiltersRunFlag{false};
    std::thread rotateFiltersThread;

    void rotateBloomFilterWorker() {
        while (rotateFiltersRunFlag) {
            std::unique_lock lock(filtersMtx);

            // 等待条件变量，但最多等待 rotationInterval 秒，期间等待 adjustBloomFilterCv 条件变量被通知
            if (adjustFiltersCv.wait_for(lock, std::chrono::seconds(rotationInterval)) == std::cv_status::no_timeout) {
                // 条件变量被通知，说明布隆过滤器参数已被更改，要重新计算周期轮换等待时间
                std::cout << "[Engine] Bloom filter parameter has been changed, rotation interval is recalculated." <<
                        std::endl;
            } else {
                // 等待超时，执行周期轮换
                filters.erase(filters.begin());
                filters.emplace_back(bloomFilterSize, hashFunctionNum);
                lock.unlock();

                // 打印信息
                const auto now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                std::ostringstream oss;
                oss << "[Engine] Rotate bloom filter at time: " << now_c;
                std::cout << oss.str() << std::endl;
            }
        }
    }
};

#endif //BLACK_LIST_ENGINE_HPP
