#pragma once

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <atomic>

inline void FutexWait(void* value, int expectedValue) {
    syscall(SYS_futex, value, FUTEX_WAIT_PRIVATE, expectedValue, nullptr, nullptr, 0);
}

inline void FutexWake(void* value, int count) {
    syscall(SYS_futex, value, FUTEX_WAKE_PRIVATE, count, nullptr, nullptr, 0);
}

class Mutex {
private:
    enum State : int {
        Unlocked = 0,
        LockedNoWaiters = 1,
        LockedWithWaiters = 2
    };

    std::atomic<int> m_state{Unlocked};

public:
    Mutex() = default;

    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

    void lock() {
        int expected = Unlocked;
        if (m_state.compare_exchange_strong(expected, LockedNoWaiters, std::memory_order_acquire)) {
            return;
        }

        if (expected != LockedWithWaiters) {
            expected = m_state.exchange(LockedWithWaiters, std::memory_order_acquire);
        }

        while (expected != Unlocked) {
            FutexWait(&m_state, LockedWithWaiters);
            expected = m_state.exchange(LockedWithWaiters, std::memory_order_acquire);
        }
    }

    void unlock() {
        if (m_state.exchange(Unlocked, std::memory_order_release) == LockedWithWaiters) {
            FutexWake(&m_state, 1);
        }
    }

    std::atomic<int>* native_handle() {
        return &m_state;
    }
};
