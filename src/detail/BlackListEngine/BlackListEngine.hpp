#ifndef BLACK_LIST_ENGINE_HPP
#define BLACK_LIST_ENGINE_HPP

#include <atomic>
#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "BaseBloomFilter.hpp"

class BlackListEngine {
public:
    BlackListEngine(const unsigned int _maxJwtLifeTime,
                    const unsigned int _rotationInterval,
                    const size_t _bloomFilterSize,
                    const unsigned int _hashFunctionNum) {
        if (_maxJwtLifeTime == 0) {
            throw std::invalid_argument("maxJwtLifeTime cannot be 0.");
        }
        if (_rotationInterval == 0) {
            throw std::invalid_argument("rotationInterval cannot be 0.");
        }
        if (_bloomFilterSize == 0) {
            throw std::invalid_argument("bloomFilterSize cannot be 0.");
        }
        if (_hashFunctionNum == 0) {
            throw std::invalid_argument("hashFunctionNum cannot be 0.");
        }
        this->maxJwtLifeTime = _maxJwtLifeTime;
        this->rotationInterval = _rotationInterval;
        this->bloomFilterSize = _bloomFilterSize;
        this->hashFunctionNum = _hashFunctionNum;

        // 计算所需布隆过滤器的个数
        this->bloomFilterNum = std::ceil(this->maxJwtLifeTime / this->rotationInterval);

        // 初始化黑名单
        this->blackList = initBlackList(this->bloomFilterNum, this->bloomFilterSize, this->hashFunctionNum);

        // 启动周期轮换线程
        if (!rotateBloomFilterThread.joinable()) {
            rotateBloomFilterThreadRunFlag.store(true);
            rotateBloomFilterThread = std::thread(&BlackListEngine::rotateBloomFilterWorker, this);
        }
    }

    ~BlackListEngine() {
        // 停止周期轮换线程
        rotateBloomFilterThreadRunFlag.store(false);
        if (rotateBloomFilterThread.joinable()) {
            rotateBloomFilterThread.join();
        }
    }

    // 初始化黑名单
    std::vector<BaseBloomFilter> initBlackList(const unsigned int blackListSize, const unsigned int bloomFilterSize,
                                               const unsigned int hashFunctionNum) const {
        // 创建新的黑名单
        std::vector<BaseBloomFilter> newBlackList;

        // 保留一定空间
        newBlackList.reserve(blackListSize);

        // 创建多个布隆过滤器
        for (unsigned int i = 0; i < this->bloomFilterNum; ++i) {
            newBlackList.emplace_back(bloomFilterSize, hashFunctionNum);
        }

        return newBlackList;
    }

    // 写入黑名单
    void write(const std::string& token, const time_t expTime) {
        // 计算这个 token 还剩多长时间过期
        const auto now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        const time_t remainingTime = expTime - now_c;

        // 防止系统时间错误（系统时间晚于过期时间，导致是负数）
        if (remainingTime <= 0) return;

        // 防止剩余时长超出 maxJwtLifeTime
        if (remainingTime > maxJwtLifeTime) return;

        // 计算需要写入到多少个布隆过滤器中
        const unsigned int num = std::ceil(remainingTime / this->rotationInterval);
        if (num > bloomFilterNum) return;

        // 分别写入到多个布隆过滤器中
        for (unsigned int i = 0; i < num; ++i) {
            blackList[i].add(token);
        }
    }

    // 查询是否在黑名单中
    bool contain(const std::string& token, const time_t expTime) const {
        // 计算这个 token 还剩多长时间过期
        const auto now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        const time_t remainingTime = expTime - now_c;

        // 防止系统时间错误（系统时间晚于过期时间，导致是负数）
        if (remainingTime <= 0) return false;

        // 防止剩余时长超出 maxJwtLifeTime
        if (remainingTime > maxJwtLifeTime) return false;

        // 计算需要查询多少个布隆过滤器
        const unsigned int num = std::ceil(remainingTime / this->rotationInterval);
        if (num > bloomFilterNum) return false;

        // 分别查询多个布隆过滤器中
        for (unsigned int i = 0; i < num; ++i) {
            // 如果任意一个布隆过滤器返回不存在，则肯定不存在于黑名单中
            if (!blackList[i].contains(token)) {
                return false;
            }
        }
        // 如果多个布隆过滤器都返回存在，则可能存在于黑名单中
        return true;
    }

    // 重建黑名单
    void rebuildBlackList() {
        // 更改布隆过滤器引擎状态

        // 创建新的布隆过滤器

        // 从文件中读取记录，写入到布隆过滤器

        std::unique_lock<std::mutex> lock(blackListMutex);
        // 返回布隆过滤器
    }

    // getter方法
    unsigned int getBloomFilterNum() const {
        return bloomFilterNum;
    }

    unsigned long getMaxJwtLifeTime() const {
        return maxJwtLifeTime;
    }

    size_t getBloomFilterSize() const {
        return bloomFilterSize;
    }

    unsigned int getHashFunctionNum() const {
        return hashFunctionNum;
    }

    unsigned long getRotationInterval() const {
        return rotationInterval;
    }

    std::vector<unsigned long> getBlackListMsgNum() const {
        std::vector<unsigned long> blackListMsgNum;
        blackListMsgNum.reserve(bloomFilterNum);
        for (const auto& baseBloomFilter : blackList) {
            blackListMsgNum.push_back(baseBloomFilter.getMsgNum());
        }
        return blackListMsgNum;
    }

private:
    // 黑名单
    std::vector<BaseBloomFilter> blackList;
    std::mutex blackListMutex;

    // 黑名单内的布隆过滤器个数
    unsigned int bloomFilterNum = 0;

    // jwt最大生存时长
    unsigned long maxJwtLifeTime = 0;

    // 每个布隆过滤器尺寸
    size_t bloomFilterSize = 0;

    // 哈希函数个数
    unsigned int hashFunctionNum = 0;

    // 周期轮换间隔
    unsigned long rotationInterval = 0;
    std::promise<unsigned int> rotationIntervalProm;
    std::future<unsigned int> rotationIntervalfut = rotationIntervalProm.get_future();

    // 周期轮换线程
    std::atomic<bool> rotateBloomFilterThreadRunFlag{false};
    std::thread rotateBloomFilterThread;

    void rotateBloomFilterWorker() {
        while (rotateBloomFilterThreadRunFlag) {
            const std::future_status status = rotationIntervalfut.wait_for(std::chrono::seconds(rotationInterval));
            // 布隆过滤器被重建了
            if (status == std::future_status::ready) {
                // 更新周期轮换间隔，并且重置等待时间
                rotationInterval = rotationIntervalfut.get();
            } else {
                // 执行周期轮换
                std::unique_lock<std::mutex> lock(blackListMutex);
                blackList.erase(blackList.begin());
                blackList.emplace_back(bloomFilterSize, hashFunctionNum);
                const auto now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                std::cout << "Rotate bloom filter at time: " << now_c << ", black list memory used: " << bloomFilterNum * bloomFilterSize / 8 / 1024 / 1024 << "MBytes" << std::endl;
            }
        }
    }
};

#endif //BLACK_LIST_ENGINE_HPP
