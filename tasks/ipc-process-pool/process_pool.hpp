#pragma once
#include <iostream>
#include <vector>
#include <atomic>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <thread>

using TaskFunc = int (*)(int);

struct ResultSlot {
    std::atomic<bool> ready{false};
    int value;
};

struct Task {
    TaskFunc func;
    int arg;
    size_t slot_idx;
};

struct PoolLayout {
    static constexpr size_t Q_SIZE = 1024;
    static constexpr size_t R_SIZE = 1024;
    std::atomic<size_t> head{0};
    std::atomic<size_t> tail{0};
    std::atomic<size_t> next_slot{0};
    Task tasks[Q_SIZE];
    ResultSlot results[R_SIZE];
};

class MyFuture {
    ResultSlot* slot;
public:
    MyFuture(ResultSlot* s) : slot(s) {}
    int get() {
        while (!slot->ready.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        return slot->value;
    }
};

class ProcessPool {
    PoolLayout* data;
    std::vector<pid_t> workers;
    bool is_main = true;
public:
    ProcessPool(size_t n) {
        data = (PoolLayout*)mmap(NULL, sizeof(PoolLayout), PROT_READ | PROT_WRITE, 
                                 MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        new (data) PoolLayout();
        for (size_t i = 0; i < n; ++i) {
            pid_t pid = fork();
            if (pid == 0) {
                is_main = false;
                worker_loop();
                exit(0);
            }
            workers.push_back(pid);
        }
    }
    MyFuture Submit(TaskFunc f, int arg) {
        size_t s_idx = data->next_slot.fetch_add(1) % PoolLayout::R_SIZE;
        data->results[s_idx].ready.store(false, std::memory_order_relaxed);
        size_t t_idx = data->tail.fetch_add(1) % PoolLayout::Q_SIZE;
        data->tasks[t_idx] = {f, arg, s_idx};
        return MyFuture(&data->results[s_idx]);
    }
    ~ProcessPool() {
        if (is_main) {
            for (pid_t p : workers) kill(p, SIGTERM);
            munmap(data, sizeof(PoolLayout));
        }
    }
private:
    void worker_loop() {
        while (true) {
            size_t h = data->head.load(std::memory_order_relaxed);
            if (h < data->tail.load(std::memory_order_acquire)) {
                if (data->head.compare_exchange_weak(h, h + 1)) {
                    Task& t = data->tasks[h % PoolLayout::Q_SIZE];
                    int res = t.func(t.arg);
                    data->results[t.slot_idx].value = res;
                    data->results[t.slot_idx].ready.store(true, std::memory_order_release);
                }
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    }
};
