#include "pch.h"
#include "Threading.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std::literals::chrono_literals;

// ----------------------------------------------------------------------------
// ThreadPoolTests
// ----------------------------------------------------------------------------
TEST_CLASS(ThreadingTests)
{
public:
    TEST_METHOD(SingleTaskReturnsResult)
    {
        ThreadPool pool(2);
        auto future = pool.submit([] { return 42; });
        Assert::AreEqual(42, future.get());
    }

    TEST_METHOD(TaskWithArgumentsReturnsCorrectSum)
    {
        ThreadPool pool(2);
        auto future = pool.submit([](int a, int b) { return a + b; }, 10, 20);
        Assert::AreEqual(30, future.get());
    }

    TEST_METHOD(MultipleTasksAllCompleteCorrectly)
    {
        ThreadPool pool(4);
        std::vector<std::future<int>> futures;

        for (int i = 0; i < 100; ++i)
            futures.push_back(pool.submit([i] { return i * i; }));

        // No waiting, futures.get() is blocking until the result is ready.
        for (int i = 0; i < 100; ++i)
            Assert::AreEqual(i * i, futures[i].get());
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
            [&future] { future.get(); }
        );
    }

    TEST_METHOD(ConcurrentSubmissionDoesNotLoseTasks)
    {
        ThreadPool pool;
        std::vector<std::future<void>> futures;
		
        // Use atomic int for thread-safe counting (ensures no race conditions
        // reading/writing to the counter variable). Since the op will be a 
        // simple increment, no need to use a full mutex.
        std::atomic<int> counter{ 0 };

        for (int i = 0; i < 1000; ++i)
        {
            futures.push_back(pool.submit([&counter] {
                // Increment the counter in the task.
                counter.fetch_add(1, std::memory_order_relaxed);
            }));
        }

        for (auto& f : futures) f.get();

		// Ensure that all 1000 tasks were executed by checking the counter value.
        Assert::AreEqual(1000, counter.load());
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
};
