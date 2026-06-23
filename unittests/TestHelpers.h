#pragma once
#include <filesystem>
#include <future>
#include <random>
#include "threading\ThreadPool.h"

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

// globalTestFile
// Return the path to a given test file in the global unittests/TestFiles folder.
inline std::filesystem::path globalTestFile(const std::string& rel_path) {
    return std::filesystem::path(__FILE__).parent_path() / "TestFiles" / rel_path;
}

// localTestFile
// Return the path to a given test file in the local TestFiles folder of the 
// calling test function's file.
inline std::filesystem::path localTestFile(
    const std::string& calling_file, 
    const std::string& rel_path
) {
    return std::filesystem::path(calling_file).parent_path() / "TestFiles" / rel_path;
}

// generateRandomString
// Used to generate unique names.
inline std::string generateRandomString(std::size_t length = 8)
{
    // Mersenne-Twister for generating the RNG seed. Constructed once (static)
    // for each thread (thread_local) to avoid different threads generating 
    // the same sequences.
    static thread_local std::mt19937_64 generator{ std::random_device{}() };
    std::uniform_int_distribution<std::uint64_t> distribution;
    return std::to_string(distribution(generator));
}

// createTestFolder
// Test utility class for creating a temporary folder, for holding any files
// created in a test function. The temporary folder, along with any contents
// is cleaned up in the class destructor, and the RAII nature of the class
// ensures that this happens no matter how a test function terminates.
class TemporaryTestFolder
{
public:
    // Constructor
    explicit TemporaryTestFolder() {
        // Create a temporary folder and store its path.
        path_ = std::filesystem::current_path()/generateRandomString();
        std::filesystem::create_directories(path_);
    }

    // Destructor
    ~TemporaryTestFolder()
    {
        // Capture the error code in a local throwaway variable.
        // We don't want to let cleanup-time failures throw from 
        // a destructor.
        std::error_code ec;

        // Delete the temporary folder and all of its contents.
        std::filesystem::remove_all(path_, ec);
    }

    // Trivial getters
    const std::filesystem::path& path() const { return path_; }

    // No copy: copying would mean two objects both believing they own,
    // and will both try to delete, the same directory.
    TemporaryTestFolder(const TemporaryTestFolder&) = delete;
    TemporaryTestFolder& operator=(const TemporaryTestFolder&) = delete;

private:
    // The temporary folder path
    std::filesystem::path path_;
};

class ThreadGuard
{
public:
    explicit ThreadGuard(std::thread& t) : thread_(t) {}
    ~ThreadGuard()
    {
        if (thread_.joinable())
            thread_.join();
    }

    ThreadGuard(const ThreadGuard&) = delete;
    ThreadGuard& operator=(const ThreadGuard&) = delete;

private:
    std::thread& thread_;
};