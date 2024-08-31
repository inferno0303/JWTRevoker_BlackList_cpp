#ifndef COROUTINE_SAFE_QUEUE_HPP
#define COROUTINE_SAFE_QUEUE_HPP

#include <boost/asio.hpp>
#include <deque>
#include <mutex>
#include <iostream>
#include <memory>

template<typename T>
class CoroutineSafeQueue {
public:
    explicit CoroutineSafeQueue(boost::asio::io_context &ioc) : ioc_(ioc) {
    }

    // 将消息放入队列中
    void enqueue(T value) {
        std::lock_guard lock(mtx_);
        queue_.push_back(std::move(value));
        if (!waiters_.empty()) {
            waiters_.front()->cancel(); // 唤醒等待的协程
            waiters_.pop_front();
        }
    }

    // 从队列中取出消息
    boost::asio::awaitable<T> dequeue() {
        for (;;) {
            std::unique_lock lock(mtx_);
            if (!queue_.empty()) {
                T value = std::move(queue_.front());
                queue_.pop_front();
                co_return value;
            }

            // 如果队列为空，设置一个定时器等待数据到来
            auto timer = std::make_unique<boost::asio::steady_timer>(ioc_);
            waiters_.push_back(std::move(timer));
            lock.unlock();
            co_await waiters_.back()->async_wait(boost::asio::use_awaitable);
        }
    }

private:
    std::mutex mtx_;
    std::deque<T> queue_;
    std::deque<std::unique_ptr<boost::asio::steady_timer> > waiters_; // 使用智能指针来管理定时器
    boost::asio::io_context &ioc_;
};

#endif //COROUTINE_SAFE_QUEUE_HPP
