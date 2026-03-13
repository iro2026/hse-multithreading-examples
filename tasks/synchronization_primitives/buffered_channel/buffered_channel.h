#pragma once

#include <optional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <stdexcept>

template <class T>
class BufferedChannel {
public:
    explicit BufferedChannel(int size) : capacity_(static_cast<size_t>(size)), is_closed_(false) {}

    void Send(const T& value) {
        std::unique_lock<std::mutex> lock(mutex_);

        push_cv_.wait(lock, [this] {
            return buffer_.size() < capacity_ || is_closed_;
        });

        if (is_closed_) {
            throw std::runtime_error("Send to closed channel");
        }

        buffer_.push(value);
        pop_cv_.notify_one();
    }

    std::optional<T> Recv() {
        std::unique_lock<std::mutex> lock(mutex_);

        pop_cv_.wait(lock, [this] {
            return !buffer_.empty() || is_closed_;
        });

        if (buffer_.empty()) {
            return std::nullopt;
        }

        T value = std::move(buffer_.front());
        buffer_.pop();
        
        push_cv_.notify_one();
        return value;
    }

    void Close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (is_closed_) {
                return;
            }
            is_closed_ = true;
        }
        push_cv_.notify_all();
        pop_cv_.notify_all();
    }

private:
    std::queue<T> buffer_;
    size_t capacity_;
    bool is_closed_;
    
    std::mutex mutex_;
    std::condition_variable push_cv_;
    std::condition_variable pop_cv_;
};
