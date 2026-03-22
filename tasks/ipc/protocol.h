#pragma once
#include <atomic>
#include <cstdint>
#include <cstddef>

namespace ipc {

inline constexpr uint32_t kVersion = 2;

struct QueueMeta {
    uint32_t version;
    uint32_t capacity;
    std::atomic<size_t> head;
    std::atomic<size_t> tail;
    std::atomic<size_t> ready_tail;
};

struct MessageHeader {
    uint32_t type;
    uint32_t length;
};

}
