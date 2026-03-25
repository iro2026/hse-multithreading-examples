#pragma once
#include <iostream>
#include <vector>
#include <atomic>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include <thread>
#include <cstring>

using TaskFunc = void (*)(const void*, void*);

struct ResultSlot {
    sem_t semaphore;
    char data[1024];
};

struct Task {
    TaskFunc func;
    char arg[1024];
    size_t slot_idx;
    std::atomic<bool> filled{false};
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

template <typename T>
class MyFuture {
    ResultSlot* slot;
public:
    MyFuture(ResultSlot* s) : slot(s) {}

    T get() {
        sem_wait(&slot->semaphore);
        T result;
        std::memcpy(&result, slot->data, sizeof(T));
        return result;
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

        for (size_t i = 0; i < PoolLayout::R_SIZE; ++i) {
            sem_init(&data->results[i].semaphore, 1, 0);
        }

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

    template <typename T_Res, typename T_Arg>
    MyFuture<T_Res> Submit(TaskFunc f, T_Arg arg) {
        while (data->tail.load() - data->head.load() >= PoolLayout::Q_SIZE) {
            std::this_thread::yield();
        }

        size_t s_idx = data->next_slot.fetch_add(1) % PoolLayout::R_SIZE;
        sem_init(&data->results[s_idx].semaphore, 1, 0);

        size_t t_idx = data->tail.fetch_add(1) % PoolLayout::Q_SIZE;
        
        data->tasks[t_idx].func = f;
        std::memcpy(data->tasks[t_idx].arg, &arg, sizeof(T_Arg));
        data->tasks[t_idx].slot_idx = s_idx;
        
        data->tasks[t_idx].filled.store(true, std::memory_order_release);

        return MyFuture<T_Res>(&data->results[s_idx]);
    }

    ~ProcessPool() {
        if (is_main) {
            for (pid_t p : workers) {
                kill(p, SIGTERM);
                waitpid(p, NULL, 0);
            }
            for (size_t i = 0; i < PoolLayout::R_SIZE; ++i) {
                sem_destroy(&data->results[i].semaphore);
            }
            munmap(data, sizeof(PoolLayout));
        }
    }

private:
    void worker_loop() {
        while (true) {
            size_t h = data->head.load(std::memory_order_relaxed);
            if (h < data->tail.load(std::memory_order_acquire)) {
                if (data->head.compare_exchange_weak(h, h + 1)) {
                    auto& t = data->tasks[h % PoolLayout::Q_SIZE];
                    
                    while (!t.filled.load(std::memory_order_acquire)) {
                        std::this_thread::yield();
                    }

                    t.func(t.arg, data->results[t.slot_idx].data);
                    
                    t.filled.store(false, std::memory_order_relaxed);
                    sem_post(&data->results[t.slot_idx].semaphore);
                }
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    }
};
