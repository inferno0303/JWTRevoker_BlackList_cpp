#ifndef THREADSAFEQUEUE_HPP
#define THREADSAFEQUEUE_HPP

#include <queue>
#include <mutex>
#include <condition_variable>

#define QUEUE_DEFAULT_MAXSIZE 1024

// 实现了线程安全（FIFO）队列
template <typename T>
class ThreadSafeQueue {
public:
    // 默认构造函数
    explicit ThreadSafeQueue() = default;

    // 构造函数：_maxSize 指定了队列最大元素个数
    explicit ThreadSafeQueue(const size_t _maxSize) {
        if (_maxSize <= 0) {
            throw std::invalid_argument("maxSize must be greater than 0");
        }
        maxSize = _maxSize;
    }

    // 向队列中添加元素
    void enqueue(T value) {
        std::unique_lock<std::mutex> lock(queueMutex);
        queueCv.wait(lock, [this] { return queue.size() < maxSize; }); // 队列满时阻塞
        queue.push(std::move(value));

        // 先释放锁，并通知可能在等待的消费者
        lock.unlock();
        queueCv.notify_all();
    }

    // 从队列中取出元素，如果队列为空，则阻塞线程，直到队列不为空
    T dequeue() {
        std::unique_lock<std::mutex> lock(queueMutex);
        queueCv.wait(lock, [this] { return !queue.empty(); }); // 等待队列非空
        T value = std::move(queue.front());
        queue.pop();

        // 先释放锁，并通知可能在等待的生产者
        lock.unlock();
        queueCv.notify_all();

        return value;
    }

    // 尝试从队列中取出元素，非阻塞，成功则返回 true，失败则返回 false，通过左值引用传递引用
    bool tryDequeue(T& value) {
        std::unique_lock<std::mutex> lock(queueMutex);
        if (queue.empty()) {
            return false;
        }
        value = std::move(queue.front());
        queue.pop();

        // 先释放锁，然后通知可能在等待的生产者
        lock.unlock();
        queueCv.notify_all();

        return true;
    }

    // 检查队列是否为空
    bool empty() const {
        std::unique_lock<std::mutex> lock(queueMutex);
        return queue.empty();
    }

    // 获取队列的大小
    size_t size() const {
        std::unique_lock<std::mutex> lock(queueMutex);
        return queue.size();
    }

private:
    size_t maxSize{QUEUE_DEFAULT_MAXSIZE}; // 最大队列大小
    std::queue<T> queue;
    mutable std::mutex queueMutex;
    std::condition_variable queueCv;
};

#endif // THREADSAFEQUEUE_HPP
