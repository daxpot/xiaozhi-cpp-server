#pragma once
#ifndef XIAOZHI_THREADSAFE_QUEUE_H
#define XIAOZHI_THREADSAFE_QUEUE_H

#include <mutex>
#include <queue>
template<typename T>
class ThreadSafeQueue {
    private:
        mutable std::mutex mutex;
        std::queue<T> data;
    public:
        ThreadSafeQueue() = default;
        ThreadSafeQueue(ThreadSafeQueue&) = delete;
        ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

        void push(T&& value) {
            std::lock_guard<std::mutex> lk(mutex);
            data.push(std::forward<T>(value));
        }
        bool try_pop(T &value) {
            std::lock_guard<std::mutex> lk(mutex);
            if(data.empty()) {
                return false;
            }
            value = std::move(data.front());
            data.pop();
            return true;
        }

        void clear() {
            std::lock_guard<std::mutex> lk(mutex);
            std::queue<T>().swap(data);
        }
        
        bool empty() const {
            std::lock_guard<std::mutex> lk(mutex);
            return data.empty();
        }
};
#endif