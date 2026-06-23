#include "pch.h"
#include "image/Image.h"

#include <thread>

// Class to be tested
#include "pipeline/PipelineQueue.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Compile-time checks
// Test class contracts (non-copyable, non-movable).
static_assert(
    !std::is_copy_constructible_v<PipelineQueue<int>>,
    "PipelineQueue must not be copyable"
    );
static_assert(
    !std::is_copy_assignable_v<PipelineQueue<int>>,
    "PipelineQueue must not be copy assignable"
    );
static_assert(
    !std::is_move_constructible_v<PipelineQueue<int>>,
    "PipelineQueue must not be moveable"
    );
static_assert(
    !std::is_move_assignable_v<PipelineQueue<int>>,
    "PipelineQueue must not be move assignable"
    );

// ----------------------------------------------------------------------------
// PipelineQueueTests
// ----------------------------------------------------------------------------
TEST_CLASS(PipelineQueueTests) {
public:
    //TEST_CLASS_INITIALIZE(ClassSetup) {}
    //TEST_CLASS_CLEANUP(ClassTeardown) {}
    TEST_METHOD_INITIALIZE(MethodSetup) {
        // Default PipelineQueue test instance.
        queue_ = std::make_unique<PipelineQueue<int>>(k_default_capacity);
    }
    TEST_METHOD_CLEANUP(MethodTeardown) {
        queue_->shutdown();
        queue_.reset();
    }

    TEST_METHOD(Construction) {
        // Test class contracts (non-copyable, non-movable).
        Assert::IsFalse(std::is_copy_constructible_v<PipelineQueue<int>>);
        Assert::IsFalse(std::is_copy_assignable_v<PipelineQueue<int>>);
        Assert::IsFalse(std::is_move_constructible_v<PipelineQueue<int>>);
        Assert::IsFalse(std::is_move_assignable_v<PipelineQueue<int>>);

        // Test the test default queue construction.
        Assert::IsFalse(queue_->isUnbounded());
        Assert::AreEqual(std::size_t(10), queue_->maxSize());

        // Test constructor input checks.
        Assert::ExpectException<std::invalid_argument>([] {
            PipelineQueue<int> queue(0);
            });

        // Test default construction.
        {
            PipelineQueue<int> queue;
            // Check that the default constructed queue is unbounded.
            Assert::IsTrue(queue.isUnbounded());
        }

        // Check the notify calls from condition_variable
    }

    TEST_METHOD(Push_UnlockPathways) {
        // Test queue has space.
        Assert::IsTrue(queue_->push(42));
        Assert::AreEqual(std::size_t(1), queue_->size());

        // Test stopping.
        queue_->shutdown();
        Assert::IsFalse(queue_->push(42));

        // Test unbounded queue.
        PipelineQueue<int> unbounded_queue;
        for (int i = 0; i < 10000; ++i)
            // std::move required by push(T&&) signature; meaningless for int
            // but necessary since the const T& overload is deleted to enforce
            // move semantics for the real Image use case.
            Assert::IsTrue(unbounded_queue.push(std::move(i)));
        unbounded_queue.shutdown();
    }

    TEST_METHOD(Pop_ItemAvailable_ReturnsItem) {
        queue_->push(42);
        auto result = queue_->pop();
        // Check that the optional has a value and 
        // that the value is what was pushed.
        Assert::IsTrue(result.has_value());
        Assert::AreEqual(42, result.value());
    }

    TEST_METHOD(Pop_StoppingWithItemsRemaining_DrainsThenReturnsNullopt) {
        // Add items, then shut down.
        queue_->push(std::move(1));
        queue_->push(std::move(2));
        queue_->shutdown();

        // Check that existing items are drained.
        Assert::AreEqual(1, *queue_->pop());
        Assert::AreEqual(2, *queue_->pop());

        // Check that calling pop on an empty queue 
        // returns an empty option (std::nullopt).
        Assert::IsFalse(queue_->pop().has_value());
    }

    TEST_METHOD(Shutdown) {
        Assert::IsFalse(queue_->isStopping());
        queue_->shutdown();
        Assert::IsTrue(queue_->isStopping());

        // Calling again should not throw or deadlock.
        queue_->shutdown();
    }

    TEST_METHOD(Functional_PopBlocksUntilItemAvailable) {
        std::optional<int> pop_return_value;
        std::atomic<bool> pop_is_blocked{ true };

        // Use a promise/future to ensure the thread has run before checking
        // the thread task results.
        std::promise<void> thread_finished;
        auto thread_finished_future = thread_finished.get_future();

        // Start a thread that will block on pop() since the queue is empty.
        std::thread consumer([&] {
            pop_return_value = queue_->pop();
            pop_is_blocked.store(false, std::memory_order_relaxed);
            thread_finished.set_value();
            });
        ThreadGuard guard(consumer);

        // Small sleep to give the consumer thread time to actually enter pop() 
        // and block.
        // Note: sleep_for is an approximation, not a hard guarantee,
        // but is sufficient for this test's purposes since 10ms is
        // generous relative to thread scheduling latency on modern hardware.
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Verify that the thread is blocked by checking that pop_is_blocked is
        // still true.
        Assert::IsTrue(pop_is_blocked.load());

        // Add an item to the queue, which should unblock pop.
        queue_->push(42);
        thread_finished_future.wait();
        Assert::IsFalse(pop_is_blocked.load());

        // Join the thread and check that the popped value is correct.
        consumer.join();
        Assert::IsTrue(pop_return_value.has_value());
        Assert::AreEqual(42, pop_return_value.value());
    }

    TEST_METHOD(Functional_PushBlocksWhenQueueFull) {
        // Create a small queue and fill it.
        PipelineQueue<int> queue(1);
        queue.push(1);

        std::atomic<bool> push_is_blocked{ true };

        // Use a promise/future to ensure the thread has run before checking
        // the thread task results.
        std::promise<void> thread_finished;
        auto thread_finished_future = thread_finished.get_future();

        // Start a thread that will block on push() since the queue is full.
        std::thread producer([&] {
            //push_started.set_value();
            queue.push(2);
            push_is_blocked.store(false, std::memory_order_relaxed);
            thread_finished.set_value();
            });
        ThreadGuard guard(producer);

        //push_started_future.wait();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Push should still be blocked.
        Assert::IsTrue(push_is_blocked.load());

        // Pop an item to free a slot. Verify that this unblocks the thread.
        queue.pop();
        thread_finished_future.wait();
        Assert::IsFalse(push_is_blocked.load());

        // Cleanup
        producer.join();
        queue.shutdown();
    }

    TEST_METHOD(Functional_ShutdownUnblocksWaitingPop) {
        std::optional<int> pop_return_value;
        std::atomic<bool> pop_is_blocked{ true };

        // Use a promise/future to ensure the thread has run before checking
        // the thread task results.
        std::promise<void> thread_finished;
        auto thread_finished_future = thread_finished.get_future();

        // Start a thread that will block on pop() since the queue is empty.
        std::thread consumer([&] {
            pop_return_value = queue_->pop();
            pop_is_blocked.store(false, std::memory_order_relaxed);
            thread_finished.set_value();
            });
        ThreadGuard guard(consumer);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Verify that the thread is blocked.
        Assert::IsTrue(pop_is_blocked.load());

        // Call shutdown to unblock pop().
        queue_->shutdown();
        thread_finished_future.wait();
        Assert::IsFalse(pop_is_blocked.load());

        // Join the thread and check that pop() returned nullopt since 
        // the queue was empty.
        consumer.join();
        Assert::IsFalse(pop_return_value.has_value());
    }

    TEST_METHOD(Functional_ShutdownUnblocksWaitingPush) {
        // Create a small queue and fill it.
        PipelineQueue<int> queue(1);
        queue.push(1);
          
        std::atomic<bool> push_return_value{ false };
        std::atomic<bool> push_is_blocked{ true };

        // Use a promise/future to ensure the thread has run before checking
        // the thread task results.
        std::promise<void> thread_finished;
        auto thread_finished_future = thread_finished.get_future();

        // Start a thread that will block on push() since the queue is full.
        std::thread producer([&] {
            bool result = queue.push(2);
            push_is_blocked.store(false, std::memory_order_relaxed);
            push_return_value.store(!result, std::memory_order_relaxed);
            thread_finished.set_value();
            });
        ThreadGuard guard(producer);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Check that push() is blocked.
        Assert::IsTrue(push_is_blocked.load());

        // Call shutdown to unblock push().
        queue.shutdown();
        thread_finished_future.wait();
        Assert::IsFalse(push_is_blocked.load());
        
        // Join the thread and check that the push was successful.
        producer.join();
        Assert::IsTrue(push_return_value.load());
    }

    TEST_METHOD(Functional_MoveSemantics_ImageNotCopied) {
        PipelineQueue<Image> image_queue;

        // Create an Image for testing.
        std::vector<unsigned char> raw_buffer(100 * 100 * 3, 128);
        Image img = makeImageFromRawBuffer(100, 100, 3, "test", raw_buffer.data());

        // Capture pixel data address before move, to verify no copy occurred.
        const unsigned char* original_data = img.pixels.data();

        // Push the Image onto the queue, then pop it out.
        image_queue.push(std::move(img));
        auto popped = image_queue.pop();

        // Check that the Image was popped with properties intact.
        Assert::IsTrue(popped.has_value());
        Assert::AreEqual(std::size_t(100), popped->width);
        Assert::AreEqual(std::size_t(100), popped->height);
        Assert::AreEqual(std::size_t(3), popped->channels);
        Assert::AreEqual(std::string("test"), popped->source_path);

        // After a move, the popped item's pixel buffer should be the same
        // heap allocation as the original, not a copy.
        Assert::IsTrue(popped->pixels.data() == original_data);

        // Cleanup
        image_queue.shutdown();
    }

private:
    std::unique_ptr<PipelineQueue<int>> queue_;
    static constexpr std::size_t k_default_capacity = 10;
};

