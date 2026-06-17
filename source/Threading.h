#pragma once
#include <assert.h>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>

class ThreadPool {
public:
	// Constructor with number of threads, defaulting to hardware concurrency.
	// It is explicit to prevent implicit conversions, ensuring that the number of threads is always specified when 
	// creating a ThreadPool instance.
	explicit ThreadPool(std::size_t numThreads_=std::thread::hardware_concurrency());
	~ThreadPool();

	// Template method to submit tasks to the thread pool. It accepts any type of callable and 
	// an arbitrary list of arguments meant for that callable.
	template<typename F, typename... F_Args>
	auto submit(F&& f, F_Args&&... args) -> std::future<std::invoke_result_t<F, F_Args...>>;

	// No copy or move since the pool owns threads
	ThreadPool(ThreadPool&&) = delete;
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	void workerLoop();

	// Worker threads
	std::vector<std::thread> workers_;

	// Task queue
	std::queue<std::function<void()>> tasks_;

	// Mutex and condition variable
	std::mutex mutex_;
	std::condition_variable cv_;

	// Flag to stop the pool
	bool stopping_{false};
};

// ----------------------------------------------------------------------------
// Template method implementation
// Must be defined in the header to avoid linker errors due to template instantiation. 
// This method allows users to submit tasks to the thread pool and returns a future that can 
// be used to retrieve the result of the task once it has been executed by a worker thread.
template<typename F, typename... F_Args>
auto ThreadPool::submit(F&& f, F_Args&&... args) -> std::future<std::invoke_result_t<F, F_Args...>> {
	// Alias the return type of the callable F with arguments Args... for convenience.
	using ReturnType = std::invoke_result_t<F, F_Args...>;

	// Bind the callable and its arguments into a packaged_task.
	// Use std::forward to preserve the value category (lvalue/rvalue) 
	// of the arguments, i.e., perfect forwarding.
	auto task = std::make_shared<std::packaged_task<ReturnType()>>(
		std::bind(std::forward<F>(f), std::forward<F_Args>(args)...)
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

		// Wrap the packaged_task in a type-erased void() lambda
		// This allows us to store tasks of any return type in the same queue, 
		// since they will all be executed as void() functions by the worker threads. 
		// The lambda captures the shared pointer to the packaged_task and calls its 
		// operator() to execute the task when the worker thread processes it.
		// Any return value of (*task)() is discarded since it is not needed in that way.
		// Instead, the real result of interest is stored in the future that we return 
		// to the caller.
		tasks_.emplace([task] { (*task)(); });
	}

	// Wake up any one worker thread to  there is a new task to take.
	cv_.notify_one();

	return future;
}