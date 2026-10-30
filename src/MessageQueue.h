#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <stop_token>

/// Thread-safe message queue with blocking receive and bounded capacity.
/// When the queue exceeds max_size, the oldest message is dropped, so senders
/// never block and receivers always see the most recent messages.
template <typename T>
class MessageQueue {
  public:
    explicit MessageQueue(std::size_t max_size = 1) : max_size_(max_size) {}

    /// Enqueue a message. Notifies one waiting receiver.
    /// If queue is at capacity, drops the oldest entry.
    void send(T msg) {
        {
            std::lock_guard lock(mtx_);
            queue_.push_back(std::move(msg));
            while (queue_.size() > max_size_) {
                queue_.pop_front();
            }
        }
        cond_.notify_one();
    }

    /// Block until a message is available, then return it via move.
    T receive() {
        std::unique_lock lock(mtx_);
        cond_.wait(lock, [this] { return !queue_.empty(); });
        return popFront();
    }

    /// Like receive(), but returns std::nullopt as soon as stop is requested
    /// on stop_token, so a blocked receiver can be shut down.
    std::optional<T> receive(std::stop_token stop_token) {
        std::unique_lock lock(mtx_);
        if (!cond_.wait(lock, stop_token, [this] { return !queue_.empty(); })) {
            return std::nullopt;
        }
        return popFront();
    }

  private:
    T popFront() {
        T msg = std::move(queue_.front());
        queue_.pop_front();
        return msg;
    }

    std::deque<T> queue_;
    std::size_t max_size_;
    std::mutex mtx_;
    // condition_variable_any is required for the stop_token-aware wait.
    std::condition_variable_any cond_;
};

#endif // MESSAGE_QUEUE_H
