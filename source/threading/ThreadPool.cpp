#include "ThreadPool.h"

// Constructor
// ----------------------------------------------------------------------------
ThreadPool::ThreadPool(std::size_t num_threads, std::size_t max_queue_size)
	: max_queue_size_(max_queue_size)
{	
	// Input checks.
	if (num_threads == 0)
		throw std::invalid_argument("ThreadPool must be created with at least one thread.");
	if (max_queue_size == 0)
		throw std::invalid_argument("ThreadPool must be created with a valid queue size.");

	// Fill up the worker container with the specified number of threads, 
	// each running the workerLoop method.
	workers_.reserve(num_threads);
	for (std::size_t i = 0; i < num_threads; ++i) {
		workers_.emplace_back(&ThreadPool::workerLoop, this);
	}
}

// Destructor
// ----------------------------------------------------------------------------
ThreadPool::~ThreadPool() {
	shutdown();
	// Wait for all worker threads to finish before destroying the pool.
	for (auto& t : workers_) t.join(); 
}

// shutDown
// ----------------------------------------------------------------------------
void ThreadPool::shutdown() {
	{
		std::lock_guard<std::mutex> lock(mutex_);
		// Signal the worker threads to stop.
		stopping_ = true;
	}
	// Wake up all worker threads to ensure they can exit if they are waiting.
	cv_.notify_all();
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

		// Increment the completed tasks counter after executing the task.
		tasks_completed_.fetch_add(1, std::memory_order_relaxed);
	}
}