#include "mutex.h"
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

TEST(MutexTest, BasicLockUnlock) {
    Mutex mtx;
    mtx.lock();
    mtx.unlock();
    SUCCEED();
}

TEST(MutexTest, ConcurrentIncrement) {
    Mutex mtx;
    int counter = 0;
    const int num_threads = 10;
    const int num_iterations = 100000;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < num_iterations; ++j) {
                mtx.lock();
                counter++;
                mtx.unlock();
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(counter, num_threads * num_iterations);
}

TEST(MutexTest, FutexWaitState) {
    Mutex mtx;
    std::atomic<bool> thread_started{false};
    std::atomic<bool> can_finish{false};

    mtx.lock();

    std::thread t([&]() {
        thread_started = true;
        mtx.lock();
        mtx.unlock();
    });

    while (!thread_started) std::this_thread::yield();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto* state = reinterpret_cast<std::atomic<int>*>(mtx.native_handle());
    EXPECT_EQ(state->load(), 2); 

    mtx.unlock();
    t.join();
    
    EXPECT_EQ(state->load(), 0);
}

TEST(MutexTest, SequentialLocking) {
    Mutex mtx;
    int stage = 0;

    mtx.lock();
    std::thread t([&]() {
        mtx.lock();
        EXPECT_EQ(stage, 1);
        stage = 2;
        mtx.unlock();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    stage = 1;
    mtx.unlock();

    t.join();
    EXPECT_EQ(stage, 2);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
