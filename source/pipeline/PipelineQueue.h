#pragma once


// TODO:
// 
// ----------------------------------------------------------------------------
// PipelineQueue
// ----------------------------------------------------------------------------
template<typename T>
class PipelineQueue {
public:
    explicit PipelineQueue(std::size_t maxSize);

    // blocks if full
    void push(T item);
    // blocks if empty; returns false if queue is shutting down and empty
    bool pop(T& outItem);
    // wake up any blocked push/pop, signal no more items coming
    void shutdown();

private:
    std::mutex mutex_;

    // Signal to producer
    std::condition_variable notFull_;
    // Signal to consumer
    std::condition_variable notEmpty_;

    // Task queue
    std::queue<T> queue_;
    std::size_t maxSize_;
    bool stopping_ = false;
};