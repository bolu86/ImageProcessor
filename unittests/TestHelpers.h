#pragma once
#include "Threading.h"
#include <future>
#include <optional>

struct BlockedTask
{
    // The blocking promise
    std::promise<void> releaseWorker;
    // The (optional) future returned by ThreadPool::submit
    std::optional<std::future<void>> future;
};

// submitBlockingTask
// Helper function to submit a task that blocks until the test releases it.
// An additional promise/future pair is used to ensure that the task has 
// started and is blocked before the test continues, which avoids race conditions
// and faulty test outcomes.
inline BlockedTask submitBlockingTask(ThreadPool& pool)
{
    // Create a promise and future to control when the first task completes.
    // Note: This pattern is much better (more deterministic) than using
    // sleep_for to try to time the test correctly.
    std::promise<void> workerStarted;
    std::future<void> workerStartedFuture = workerStarted.get_future();

    BlockedTask result;
    // Need to use shared_future here since we need to be able to copy it
    // into the lambda for the worker task (std::future is not copyable).
    // Cannot use reference like with workerStarted since releaseSignal
    // must live beyond BlockedTask.
    std::shared_future<void> releaseSignal = result.releaseWorker.get_future();

    result.future = pool.submit([releaseSignal, &workerStarted] {
        // Signal that the worker has started.
        workerStarted.set_value();
        // Block the thread execution.
        releaseSignal.wait();
        });

    // Wait until the worker thread has actually started.
    workerStartedFuture.wait();

    return result;
}