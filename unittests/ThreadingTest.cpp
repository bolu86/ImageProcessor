#include "pch.h"
#include "Threading.h"

// Compile-time checks
static_assert(
    !std::is_copy_constructible_v<ThreadPool>,
    "ThreadPool must not be copyable"
   );
static_assert(
    !std::is_copy_assignable_v<ThreadPool>,
    "ThreadPool must not be copy assignable"
    );
static_assert(
    !std::is_move_constructible_v<ThreadPool>,
    "ThreadPool must not be moveable"
   );
static_assert(
    !std::is_move_assignable_v<ThreadPool>,
    "ThreadPool must not be move assignable"
    );

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std::literals::chrono_literals;

// ----------------------------------------------------------------------------
// ThreadPoolTests
// ----------------------------------------------------------------------------
TEST_CLASS(ThreadingTests)
{
public:
    TEST_METHOD(Construction) 
    {
        Assert::IsFalse(std::is_copy_constructible_v<ThreadPool>);
        Assert::IsFalse(std::is_copy_assignable_v<ThreadPool>);
        Assert::IsFalse(std::is_move_constructible_v<ThreadPool>);
        Assert::IsFalse(std::is_move_assignable_v<ThreadPool>);

        // Default construction
        {
            ThreadPool pool;
            size_t default_number_of_threads = std::thread::hardware_concurrency();
            Assert::AreEqual(default_number_of_threads, pool.getNumThreads());
            size_t default_max_queue_size = 100;
            Assert::AreEqual(default_max_queue_size, pool.getMaxQueueSize());
        }

		// Parameterized construction
        {
            ThreadPool pool(4, 10);
            Assert::AreEqual(static_cast<size_t>(4), pool.getNumThreads());
            Assert::AreEqual(static_cast<size_t>(10), pool.getMaxQueueSize());
        }
    }

    TEST_METHOD(SubmitSingleTask)
    {
        ThreadPool pool;
        auto future = pool.submit([] { return 42; });
        Assert::AreEqual(42, future->get());
    }

    TEST_METHOD(QueueEmptiesOnThreadTakingTask)
    {
        ThreadPool pool;

        std::promise<void> releaseWorker;
        std::shared_future<void> releaseSignal = releaseWorker.get_future();

        std::promise<void> workerStarted;
        std::shared_future<void> workerStartedFuture = workerStarted.get_future();

        auto blocker = pool.submit([releaseSignal, &workerStarted] {
            workerStarted.set_value();
            releaseSignal.wait();
        });

        workerStartedFuture.wait();

        //Assert::AreEqual(static_cast<std::size_t>(0), pool.getQueueSize());

        // Cleanup: release the blocked worker so the pool can shut down cleanly
        releaseWorker.set_value();
    }

    TEST_METHOD(TaskWithArguments)
    {
        ThreadPool pool;
        auto future = pool.submit([](int a, int b) { return a + b; }, 10, 20);
        Assert::AreEqual(30, future->get());
    }

    TEST_METHOD(MultipleTasksAllCompleteCorrectly)
    {
        ThreadPool pool(4);
        std::vector<std::optional<std::future<int>>> futures;

        for (int i = 0; i < 100; ++i)
            futures.push_back(pool.submit([i] { return i * i; }));

        // No waiting, futures.get() is blocking until the result is ready.
        for (int i = 0; i < 100; ++i)
            Assert::AreEqual(i * i, futures[i]->get());
    }

    TEST_METHOD(ExceptionPropagatesThroughFuture)
    {
        ThreadPool pool(2);
        auto future = pool.submit(
		    // No return value, can't auto-deduce type, so specify it explicitly as int.
            []() -> int {throw std::runtime_error("some error");}
        );

        // Ensure the task exception was propagated to the future.
        Assert::ExpectException<std::runtime_error>(
            // Defer execution so that ExpectException triggers the exception internally.
            [&future] { future->get(); }
        );
    }

    TEST_METHOD(ConcurrentSubmissionDoesNotLoseTasks)
    {
		// Use a large task queue to avoid hitting the max queue size limit 
        // during this test, which would cause tasks to be dropped and lead 
        // to false negatives.
        ThreadPool pool(8, 2000);
        std::vector<std::optional<std::future<void>>> futures;
		
        // Use atomic int for thread-safe counting (ensures no race conditions
        // reading/writing to the counter variable). Since the op will be a 
        // simple increment, no need to use a full mutex.
        std::atomic<int> counter{ 0 };
        int dropped{};

        for (int i = 0; i < 1000; ++i)
        {
            auto result = pool.submit([&counter] {
                // Increment the counter in the task.
                counter.fetch_add(1, std::memory_order_relaxed);
            });

			// Only add the future to the list if the task was accepted into the queue...
			if (result.has_value())
				futures.push_back(std::move(result));
            // ...otherwise track the dropped tasks.
			else
				++dropped;
        }

		// Wait for all tasks to complete by calling get() on all the futures.
        for (auto& f : futures) f->get();

		// Ensure that all 1000 tasks were executed by checking the counter value + any
        // dropped tasks.
        Assert::AreEqual(1000, counter.load() + dropped);
    }

    TEST_METHOD(PoolShutsDownCleanlyWithPendingTasks)
    {
        std::atomic<int> completedCount{ 0 };

		// Scope to ensure the pool is destroyed before we check the count.
        {
            ThreadPool pool(2);
            for (int i = 0; i < 20; ++i)
            {
                pool.submit([&completedCount] {
                    std::this_thread::sleep_for(5ms);
                    completedCount.fetch_add(1, std::memory_order_relaxed);
                });
            }
            // pool destructor runs here, must not deadlock.
        }

        Assert::AreEqual(20, completedCount.load());
    }

    TEST_METHOD(SubmitAfterDestructionThrows)
    {
        // We can't keep using the pool after it's destroyed, so we need
        // a way to force stopping_ = true while the object is still alive.
        ThreadPool pool(2);

        // Shut down the pool.
        pool.shutdown();

        // Verify that we are shutting down.
		Assert::IsTrue(pool.isStopping());

        // Submitting a new task should now trigger an exception.
        Assert::ExpectException<std::runtime_error>([&pool] {
            pool.submit([] { return 1; });
        });
    }

    TEST_METHOD(SubmitSucceedsWhenQueueHasSpace)
    {
        // Create a pool with 1 thread and max 10 tasks.
        ThreadPool pool(1, 10);
        auto result = pool.submit([] { return 42; });

        // Check that the returned std::optional has a value.
        Assert::IsTrue(result.has_value());
        // Check that the future it contains returns the expected result.
        Assert::AreEqual(42, result->get());
    }

    TEST_METHOD(SubmitFailsWhenQueueFull)
    {
        // Create a pool with 1 thread and max 2 tasks so that it easily backs up.
        ThreadPool pool(1, 2);
        
		// Create a promise and future to control when the first task completes.
        // Note: This pattern is much better (more deterministic) than using
		// sleep_for to try to time the test correctly.
        std::promise<void> releaseWorker;
        // Need to use shared_future here since we need to be able to copy it
		// into the lambda for the worker task (std::future is not copyable).
        std::shared_future<void> releaseSignal = releaseWorker.get_future();

        std::promise<void> workerStarted;
        std::shared_future<void> workerStartedFuture = workerStarted.get_future();

        // Occupy the only worker thread and block it.
        auto blocker = pool.submit([releaseSignal, &workerStarted] {
			workerStarted.set_value();
            // Blocks until the test releases it.
            releaseSignal.wait();   
        });

		workerStartedFuture.wait();

        // Fill up the queue with trivial tasks.
        auto t2 = pool.submit([] { return 1; });
        auto t3 = pool.submit([] { return 2; });

        // Check that the trivial tasks were accepted into the queue by
		// checking that they return valid optionals.
        Assert::IsTrue(t2.has_value());
        Assert::IsTrue(t3.has_value());

        auto t4 = pool.submit([] { return 3; });

        // Queue is full, subsequent tasks should be rejected.
        Assert::IsFalse(t4.has_value());

        // Cleanup: release the blocked worker so the pool can shut down cleanly
        releaseWorker.set_value();
    }
};
