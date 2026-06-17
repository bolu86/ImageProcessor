#include "Threading.h"

// Constructor
// ----------------------------------------------------------------------------
ThreadPool::ThreadPool(std::size_t numThreads) {
	// Ensure that the pool is created with a valid number of worker threads.
	assert(numThreads > 0);

	// Fill up the worker container with the specified number of threads, 
	// each running the workerLoop method.
	workers_.reserve(numThreads);
	for (std::size_t i = 0; i < numThreads; ++i) {
		workers_.emplace_back(&ThreadPool::workerLoop, this);
	}
}

// Destructor
// ----------------------------------------------------------------------------
ThreadPool::~ThreadPool() {
	{
		std::lock_guard<std::mutex> lock(mutex_);
		// Signal the worker threads to stop.
		stopping_ = true; 
	}
	// Wake up all worker threads to ensure they can exit if they are waiting.
	cv_.notify_all(); 

	// Wait for all worker threads to finish before destroying the pool.
	for (auto& t : workers_) t.join(); 
}

// workerLoop
// ----------------------------------------------------------------------------
void ThreadPool::workerLoop() {
	// Loop until stop conditions are met.
	while (true) {
		// Create a temporary variable for the task that will be executed.
		std::function<void()> work{};
		{
			// std::unique_lock is heavier than std::lock_guard but provides features 
			// like manual unlocking, deferred locking, and condition variable support.
			// This is needed on the next line to wait on the condition variable while 
			// holding the lock.
			std::unique_lock<std::mutex> lock(mutex_);

			// Wait until there is a task to execute or the pool is stopping.
			cv_.wait(lock, [this] { return !tasks_.empty() || stopping_; });

			// If the pool is stopping and there are no tasks left, exit the loop.
			if (stopping_ && tasks_.empty()) return;

			// Get the next task from the queue by moving it into the temporary variable.
			work = std::move(tasks_.front());
			tasks_.pop();
		}

		// Execute the task outside of the lock to allow other threads to access the queue.
		work();
	}
}