#ifndef THREAD_SAFE_QUEUE_HPP
#define THREAD_SAFE_QUEUE_HPP

#include <queue>
#include <mutex>
#include <condition_variable>

#define QUEUE_DEFAULT_MAXSIZE 4096

template<typename T>
class ThreadSafeQueue {
public:
    explicit ThreadSafeQueue(const size_t _maxSize = QUEUE_DEFAULT_MAXSIZE) : maxSize(_maxSize) {
    }

    // 向队列中添加元素
    void enqueue(T value) {
        std::unique_lock lock(queueMutex);
        queueCv.wait(lock, [this] { return queue.size() < maxSize; }); // 队列满时等待
        queue.push(std::move(value));
        queueCv.notify_one();
    }

    // 从队列中取出元素
    T dequeue() {
        std::unique_lock lock(queueMutex);
        queueCv.wait(lock, [this] { return !queue.empty(); }); // 等待队列中有元素
        T value = std::move(queue.front());
        queue.pop();
        queueCv.notify_one();
        return value;
    }

    // 检查队列是否为空
    bool isEmpty() const {
        std::unique_lock lock(queueMutex);
        return queue.empty();
    }

    // 获取队列的大小
    size_t size() const {
        std::unique_lock lock(queueMutex);
        return queue.size();
    }

private:
    size_t maxSize;
    std::queue<T> queue;
    mutable std::mutex queueMutex;
    std::condition_variable queueCv;
};

#endif // THREAD_SAFE_QUEUE_HPP
