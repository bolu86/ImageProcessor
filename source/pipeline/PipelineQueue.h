#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

// TODO:
// 
// ----------------------------------------------------------------------------
// PipelineQueue
// ----------------------------------------------------------------------------
template<typename T>
class PipelineQueue {
public:
    explicit PipelineQueue(std::size_t max_size = kUnbounded)
        :max_size_(max_size) {

        // Input checks.
        if (max_size == 0)
            throw std::invalid_argument(
                "PipelineQueue: max_size must be greater than 0"
            );
    }

    // Pushes an item into the queue, blocking if the queue is full.
    // Returns false immediately if the queue is shutting down.
    bool push(T&& item) {
        std::unique_lock<std::mutex> lock(mutex_);
                
        // Producesrs wait if the queue is full. Continue if the queue 
        // is not full, we are stopping, or the queue is unbounded.
        producers_.wait(lock, [this] {
            return stopping_ || isUnbounded() || queueHasSpace();
            });

        // Stop adding items and return.
        if (stopping_)
            return false;

        // Add new item to the back of the queue. Note! Always move 
        // since item is assumed to be large.
        queue_.push(std::move(item));

        // Notify consumers that a new item is available for processing.
        consumers_.notify_one();

        return true;
    }

    // Explicitly reject const references since any const value cannot be
    // moved, and we want to enforce move semantics for the expectedly
    // large Image objects.
    bool push(const T& item) = delete;

    // Pops an item from the queue, blocking if the queue is empty.
    // Returns std::nullopt if the queue is shut down and empty.
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);

        // Consumers wait if queue is empty. Continue if there are
        // items in the queue or we are stopping.
        consumers_.wait(lock, [this] {
            return stopping_ || !queue_.empty();
            });

        // Return null option if the queue is empty. Note the absence of 
        // stopping_ handling. On stopping, We want the consumers to empty 
        // the queue normally.
        if (queue_.empty())
            return std::nullopt;

        // Take the first item in the queue, per FIFO rules.
        T item = std::move(queue_.front());
        queue_.pop();

        // Notify producers that a slot in the queue has opened.
        producers_.notify_one();

        return item;
    }

    // Signals no more items will be pushed. Unblocks any waiting pop() calls.
    void shutdown() {
        // Set the stopping flag before notifying producers and consumers.
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }

        // Notify producers and consumers.
        producers_.notify_all();
        consumers_.notify_all();
    }

    // Return the current size of the queue.
    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    bool isStopping() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stopping_;
    }

    // Trivial getters
    std::size_t maxSize() const { return max_size_; }
    bool isUnbounded() const { return max_size_ == kUnbounded; }

    // This class does not allow copy or move.
    PipelineQueue(const PipelineQueue&) = delete;
    PipelineQueue& operator=(const PipelineQueue&) = delete;
    PipelineQueue(PipelineQueue&&) = delete;
    PipelineQueue& operator=(PipelineQueue&&) = delete;

private:
    // Helper function to check if the queue currently has space for 
    // more items. Note! Because it queries a property of the queue,
    // it must be called under mutex lock.
    bool queueHasSpace() { return queue_.size() < max_size_; }

    // Sentinal for an "unbounded" task queue.
    static constexpr std::size_t kUnbounded = std::numeric_limits<std::size_t>::max();

    mutable std::mutex mutex_;

    // Signal to producer
    std::condition_variable producers_;
    // Signal to consumer
    std::condition_variable consumers_;

    // Task queue
    std::queue<T> queue_;
    std::size_t max_size_;
    bool stopping_{ false };
};