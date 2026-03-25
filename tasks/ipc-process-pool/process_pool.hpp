#pragma once
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <signal.h>
#include <iostream>

const uint32_t MAGIC = 0xDEADBEEF;

struct MessageHeader {
    uint32_t magic;
    uint64_t id;
    uint32_t task_type;
    uint64_t data_size; 
};

class ProcessPool {
    struct Worker { pid_t pid; int write_fd; };

    static bool FullRead(int fd, void* buf, size_t len) {
        size_t r = 0;
        while (r < len) {
            ssize_t n = read(fd, (char*)buf + r, len - r);
            if (n <= 0) return false;
            r += n;
        }
        return true;
    }

    static void FullWrite(int fd, const void* buf, size_t len) {
        size_t s = 0;
        while (s < len) {
            ssize_t n = write(fd, (const char*)buf + s, len - s);
            if (n <= 0) break;
            s += n;
        }
    }

public:
    using ProcessorFunc = std::function<std::vector<uint8_t>(uint32_t, const std::vector<uint8_t>&)>;

    struct Future {
        std::mutex m;
        std::condition_variable cv;
        std::vector<uint8_t> result;
        bool ready = false;
        std::vector<uint8_t> Get() {
            std::unique_lock<std::mutex> l(m);
            cv.wait(l, [this]{ return ready; });
            return std::move(result);
        }
    };

    ProcessPool(size_t n, ProcessorFunc proc) {
        int res_p[2]; pipe(res_p);
        res_read_fd = res_p[0];
        
        for (size_t i = 0; i < n; ++i) {
            int task_p[2]; pipe(task_p);
            pid_t pid = fork();
            if (pid == 0) {
                close(task_p[1]); close(res_read_fd);
                worker_loop(task_p[0], res_p[1], proc);
                exit(0);
            }
            close(task_p[0]);
            workers.push_back({pid, task_p[1]});
        }
        close(res_p[1]);
        collector = std::thread(&ProcessPool::collect, this);
    }

    std::shared_ptr<Future> Submit(uint32_t type, const std::vector<uint8_t>& data) {
        uint64_t id = next_id++;
        auto f = std::make_shared<Future>();
        { std::lock_guard<std::mutex> l(map_mtx); fut_map[id] = f; }
        
        MessageHeader h{MAGIC, id, type, (uint64_t)data.size()};
        FullWrite(workers[id % workers.size()].write_fd, &h, sizeof(h));
        if (h.data_size > 0) FullWrite(workers[id % workers.size()].write_fd, data.data(), data.size());
        return f;
    }

    ~ProcessPool() {
        for (auto& w : workers) { close(w.write_fd); kill(w.pid, SIGTERM); waitpid(w.pid, NULL, 0); }
        if (collector.joinable()) collector.join();
    }

private:
    void worker_loop(int r, int w, ProcessorFunc proc) {
        MessageHeader h;
        while (FullRead(r, &h, sizeof(h))) {
            if (h.magic != MAGIC) continue; 

            std::vector<uint8_t> in(h.data_size);
            if (h.data_size > 0) FullRead(r, in.data(), h.data_size);
            
            auto out = proc(h.task_type, in);
            
            MessageHeader rh{MAGIC, h.id, 0, (uint64_t)out.size()};
            
            std::vector<uint8_t> packet(sizeof(rh) + out.size());
            std::memcpy(packet.data(), &rh, sizeof(rh));
            if (!out.empty()) std::memcpy(packet.data() + sizeof(rh), out.data(), out.size());
            
            FullWrite(w, packet.data(), packet.size());
        }
    }

    void collect() {
        MessageHeader h;
        while (FullRead(res_read_fd, &h, sizeof(h))) {
            if (h.magic != MAGIC) {
                char dummy;
                while (read(res_read_fd, &dummy, 1) > 0) {}
                continue;
            }

            std::vector<uint8_t> d(h.data_size);
            if (h.data_size > 0) FullRead(res_read_fd, d.data(), h.data_size);
            
            std::shared_ptr<Future> f;
            {
                std::lock_guard<std::mutex> l(map_mtx);
                if (fut_map.count(h.id)) {
                    f = fut_map[h.id];
                    fut_map.erase(h.id);
                }
            }
            if (f) {
                {
                    std::lock_guard<std::mutex> l(f->m);
                    f->result = std::move(d);
                    f->ready = true;
                }
                f->cv.notify_all();
            }
        }
    }

    std::vector<Worker> workers;
    int res_read_fd;
    std::atomic<uint64_t> next_id{0};
    std::unordered_map<uint64_t, std::shared_ptr<Future>> fut_map;
    std::mutex map_mtx;
    std::thread collector;
};
