#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>

/// Thread-safe message queue with blocking receive and bounded capacity.
/// When the queue exceeds max_size, the oldest message is dropped (back-pressure).
template <typename T>
class MessageQueue {
  public:
    explicit MessageQueue(std::size_t max_size = 1) : max_size_(max_size) {}

    /// Enqueue a message. Notifies one waiting receiver.
    /// If queue is at capacity, drops the oldest entry.
    void send(T &&msg) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            queue_.push_back(std::move(msg));
            while (queue_.size() > max_size_) {
                queue_.pop_front();
            }
        }
        cond_.notify_one();
    }

    /// Overload for lvalue references — copies then delegates to rvalue overload.
    void send(const T &msg) {
        T copy = msg;
        send(std::move(copy));
    }

    /// Block until a message is available, then return it via move.
    T receive() {
        std::unique_lock<std::mutex> lock(mtx_);
        cond_.wait(lock, [this] { return !queue_.empty(); });
        T msg = std::move(queue_.front());
        queue_.pop_front();
        return msg;
    }

  private:
    std::deque<T> queue_;
    std::size_t max_size_;
    std::mutex mtx_;
    std::condition_variable cond_;
};

#endif // MESSAGE_QUEUE_H
