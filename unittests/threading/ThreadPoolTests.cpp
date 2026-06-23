#include "pch.h"

// Class to be tested
#include "threading\ThreadPool.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std::literals::chrono_literals;

// Compile-time checks
// Test class contracts (non-copyable, non-movable).
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

// ----------------------------------------------------------------------------
// ThreadPoolTests
// ----------------------------------------------------------------------------
TEST_CLASS(ThreadPoolTests)
{
public:
    TEST_CLASS_INITIALIZE(ClassSetup) {
        // Fetch the hardware concurrency once and ensure it is a valid value.
        hardware_concurrency_ = std::max<std::size_t>(1, std::thread::hardware_concurrency());
    }
    //TEST_CLASS_CLEANUP(ClassTeardown) {}
    TEST_METHOD_INITIALIZE(MethodSetup) {
        // Default ThreadPool test instance.
        pool_ = std::make_unique<ThreadPool>(hardware_concurrency_, 1000);
    }
    TEST_METHOD_CLEANUP(MethodTeardown) {
		// Reset the pool to ensure it is destroyed before the next test starts.
		pool_.reset();
    }

    TEST_METHOD(Construction) 
    {   
		// Test class contracts (non-copyable, non-movable).
        Assert::IsFalse(std::is_copy_constructible_v<ThreadPool>);
        Assert::IsFalse(std::is_copy_assignable_v<ThreadPool>);
        Assert::IsFalse(std::is_move_constructible_v<ThreadPool>);
        Assert::IsFalse(std::is_move_assignable_v<ThreadPool>);

		// Test invalid construction parameters.
		Assert::ExpectException<std::invalid_argument>(
            [] {ThreadPool pool(0, 10);}
        );
		Assert::ExpectException<std::invalid_argument>(
            [] {ThreadPool pool(4, 0);}
        );

        // Test construction.
        {
            // Check that the given number of worker threads were created.
            ThreadPool pool(4, 10);
            Assert::AreEqual(static_cast<size_t>(4), pool.getNumThreads());
        }
    }

    TEST_METHOD(SubmitSingleTask)
    {
        auto result = pool_->submit([] { return 42; });

        // Check that the returned std::optional has a value.
        Assert::IsTrue(result.has_value());
        // Check that the future it contains returns the expected result.
        Assert::AreEqual(42, result->get());
    }

    TEST_METHOD(TaskWithArguments)
    {
        auto future = pool_->submit([](int a, int b) { return a + b; }, 10, 20);
        Assert::AreEqual(30, future->get());
    }

    TEST_METHOD(QueueEmptiesOnWorkerTakingTask)
    {
		auto blocker = submitBlockingTask(*pool_);

        // Ensure that the task queue popped the added task.
        Assert::AreEqual(static_cast<std::size_t>(0), pool_->getQueueSize());

        // Cleanup: release the blocked worker so the pool can shut down cleanly.
        blocker.releaseWorker.set_value();
    }

    TEST_METHOD(MultipleTasksAllCompleteCorrectly)
    {
        std::vector<std::optional<std::future<int>>> futures;

        for (int i = 0; i < 100; ++i)
            futures.push_back(pool_->submit([i] { return i * i; }));

        // futures.get() is blocking until the result is ready, 
        // ensuring all tasks complete successively.
        for (int i = 0; i < 100; ++i)
            Assert::AreEqual(i * i, futures[i]->get());
    }

    TEST_METHOD(ExceptionPropagatesThroughFuture)
    {
        auto future = pool_->submit(
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
        ThreadPool pool(hardware_concurrency_, 2000);
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
            ThreadPool pool(2, 1000);
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
        // Shut down the pool.
        pool_->shutdown();

        // Verify that we are shutting down.
		Assert::IsTrue(pool_->isStopping());

        // Submitting a new task should now trigger an exception.
        Assert::ExpectException<std::runtime_error>([this] {
            pool_->submit([] { return 1; });
        });
    }

    TEST_METHOD(SubmitFailsWhenQueueFull)
    {
        // Create a pool with 1 thread and max 2 tasks so that it easily backs up.
        ThreadPool pool(1, 2);

		auto blocked = submitBlockingTask(pool);

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
        blocked.releaseWorker.set_value();
    }

    TEST_METHOD(Metrics)
    {
        ThreadPool pool(1, 1);
		auto blocked = submitBlockingTask(pool);

        // Add a task to the queue to fill it.
        auto t2 = pool.submit([] { return 1; });
        // Adding another task should lead to a rejection.
        auto t3 = pool.submit([] { return 2; });

		// Check that t2 was accepted and t3 was rejected.
        Assert::IsTrue(t2.has_value());
        Assert::IsFalse(t3.has_value());

        // Release the first task to allow t2 to also complete.
        blocked.releaseWorker.set_value();
        blocked.future->get();
        t2->get();

        Assert::AreEqual(static_cast<std::uint64_t>(2), pool.getTasksSubmitted());
        Assert::AreEqual(static_cast<std::uint64_t>(1), pool.getTasksRejected());
		Assert::AreEqual(static_cast<std::uint64_t>(2), pool.getTasksCompleted());
    }

private:
    static std::size_t hardware_concurrency_;
    std::unique_ptr<ThreadPool> pool_;
};

// Define static member variable default values.
std::size_t ThreadPoolTests::hardware_concurrency_{ 0 };
