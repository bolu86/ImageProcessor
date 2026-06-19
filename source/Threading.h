#pragma once
#include <assert.h>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>

// TODO:
// - Task priorities, by using a std::priority_queue for the tasks.
// - Task timeouts + watchdog for handling stuck tasks.
// - forceShutDown() member function for hard stops?

class ThreadPool {
public:
	// Constructor.
	// It is explicit to prevent implicit conversions, ensuring that 
	// input parameters are always specified when creating a 
	// ThreadPool instance.
	explicit ThreadPool(std::size_t numThreads, std::size_t maxQueueSize);
	~ThreadPool();

	// Template method to submit tasks to the thread pool. It accepts any type of callable and 
	// an arbitrary list of arguments meant for that callable.
	template<typename F, typename... F_Args>
	auto submit(F&& f, F_Args&&... args) -> std::optional<std::future<std::invoke_result_t<F, F_Args...>>>;

	// Shutdown procedure, used to end all threads gracefully.
	void shutdown();

	// No copy or move since the pool owns threads
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

	// Since the compiler never generates move operations if the copy operations are deleted, 
	// we do not need to explicitly delete the move operations. However, it would be good
	// practice to explicitly delete them anyway, but it is left commented out here for now.
	//ThreadPool(ThreadPool&&) = delete;
	//ThreadPool& operator=(ThreadPool&&) = delete;

	// Get a snapshot of the current queue size. 
	// This is not thread-safe and may be inaccurate, 
	// but it can be useful for monitoring or 
	// debugging purposes.
	std::size_t getQueueSize() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return tasks_.size();
	}

	// Trivial getters
	std::size_t getNumThreads() const { return workers_.size(); }
	std::size_t getMaxQueueSize() const { return maxQueueSize_; }
	bool isStopping() const { return stopping_; }
	std::uint64_t getTasksSubmitted() const { return tasksSubmitted_.load(); }
	std::uint64_t getTasksRejected() const { return tasksRejected_.load(); }
	std::uint64_t getTasksCompleted() const { return tasksCompleted_.load(); }


private:
	void workerLoop();

	// Mutex and condition variable
	mutable std::mutex mutex_;
	std::condition_variable cv_;

	// Worker threads
	std::vector<std::thread> workers_;

	// Task queue
	std::queue<std::function<void()>> tasks_;

	// Max tasks to accept, for handling back-pressure under high load.
	std::size_t maxQueueSize_{};

	// Flag to stop the pool
	bool stopping_{ false };

	// Metrics
	std::atomic<std::uint64_t> tasksSubmitted_{ 0 };
	std::atomic<std::uint64_t> tasksRejected_{ 0 };
	std::atomic<std::uint64_t> tasksCompleted_{ 0 };
};

// ----------------------------------------------------------------------------
// Template method implementation
// Must be defined in the header to avoid linker errors due to template instantiation. 
// This method allows users to submit tasks to the thread pool and returns a future that can 
// be used to retrieve the result of the task once it has been executed by a worker thread.
template<typename F, typename... F_Args>
auto ThreadPool::submit(F&& f, F_Args&&... f_args) 
	-> std::optional<std::future<std::invoke_result_t<F, F_Args...>>>
{
	// Alias the return type of the callable F with arguments Args... for convenience.
	using ReturnType = std::invoke_result_t<F, F_Args...>;

	// Prepare a task to put into the queue.
	// 1. Wrap the callable and arguments into a lambda so that they can be called later by a worker.
	// 2. Wrap the lambda into a packaged_task. This allows us to get a future for the result of the task.
	// 3. Wrap the packaged_task in a shared_pointer since packaged_task does not support copying.
	auto task = std::make_shared<std::packaged_task<ReturnType()>>(
		// Use std::forward to preserve the value category (lvalue/rvalue) of the arguments, 
		// i.e., perfect forwarding. Use mutable because the lambda capture-by-value adds 
		// const qualifiers by default.
		[f = std::forward<F>(f), f_args = std::make_tuple(std::forward<F_Args>(f_args)...)]() mutable {
			// Uniform way to call the callable with its arguments.
			return std::apply(f, f_args);
		}
	);

	// Get the future from the packaged_task to return to the caller.
	std::future<ReturnType> future = task->get_future();

	// Scope to ensure the lock_guard unlocks automatically when it goes out of scope.
	{
		std::lock_guard<std::mutex> lock(mutex_);

		// Check if the pool is stopping before adding a new task. If it is, throw an 
		// exception to prevent adding tasks to a stopped pool, which would lead to 
		// undefined behavior when worker threads try to execute tasks.
		if (stopping_)
			throw std::runtime_error("submit() called on a stopped ThreadPool");

		if (tasks_.size() >= maxQueueSize_) {
			// Track the rejected task for metrics purposes.
			tasksRejected_.fetch_add(1, std::memory_order_relaxed);

			// Fail fast if queue is full. Let caller handle what to do, e.g. drop data.
			return std::nullopt;
		}

		// Wrap the task pointer in a type-erased void() lambda to match the element type of the queue.
		// Any return value of (*task)() is discarded since it is not needed in that way.
		// Instead, the real result of interest is stored in the future that we return 
		// to the caller.
		tasks_.emplace([task] { (*task)(); });
	}

	// Wake up any one worker thread to  there is a new task to take.
	cv_.notify_one();

	// Track the submitted task for metrics purposes.
	tasksSubmitted_.fetch_add(1, std::memory_order_relaxed);

	return future;
}