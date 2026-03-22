#pragma once
#include "protocol.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <optional>

namespace ipc {

class BaseNode {
protected:
    int fd_ = -1;
    size_t total_size_ = 0;
    void* mmap_ptr_ = nullptr;
    QueueMeta* meta_ = nullptr;
    uint8_t* buffer_ = nullptr;

    void map_shared_memory(const char* path, size_t capacity, bool create) {
        int flags = create ? (O_CREAT | O_RDWR) : O_RDWR;
        fd_ = shm_open(path, flags, 0666);
        if (fd_ == -1) throw std::runtime_error("shm_open failed");

        if (create) {
            total_size_ = sizeof(QueueMeta) + capacity;
            if (ftruncate(fd_, total_size_) == -1) throw std::runtime_error("ftruncate failed");
        } else {
            struct stat st;
            if (fstat(fd_, &st) == -1) throw std::runtime_error("fstat failed");
            total_size_ = st.st_size;
        }

        mmap_ptr_ = mmap(nullptr, total_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (mmap_ptr_ == MAP_FAILED) throw std::runtime_error("mmap failed");

        meta_ = static_cast<QueueMeta*>(mmap_ptr_);
        buffer_ = static_cast<uint8_t*>(mmap_ptr_) + sizeof(QueueMeta);

        if (create) {
            meta_->version = kVersion;
            meta_->capacity = static_cast<uint32_t>(capacity);
            meta_->head.store(0);
            meta_->tail.store(0);
            meta_->ready_tail.store(0);
        } else {
            if (meta_->version != kVersion) throw std::runtime_error("protocol version mismatch");
        }
    }

    void unmap_shared_memory() {
        if (mmap_ptr_) munmap(mmap_ptr_, total_size_);
        if (fd_ != -1) close(fd_);
    }

    void ring_write(size_t pos, const void* src, size_t size) {
        size_t cap = meta_->capacity;
        size_t offset = pos % cap;
        size_t first_chunk = std::min(size, cap - offset);
        std::memcpy(buffer_ + offset, src, first_chunk);
        if (size > first_chunk) {
            std::memcpy(buffer_, static_cast<const uint8_t*>(src) + first_chunk, size - first_chunk);
        }
    }

    void ring_read(size_t pos, void* dest, size_t size) {
        size_t cap = meta_->capacity;
        size_t offset = pos % cap;
        size_t first_chunk = std::min(size, cap - offset);
        std::memcpy(dest, buffer_ + offset, first_chunk);
        if (size > first_chunk) {
            std::memcpy(static_cast<uint8_t*>(dest) + first_chunk, buffer_, size - first_chunk);
        }
    }
};

class ProducerNode : public BaseNode {
public:
    ProducerNode(const char* path, size_t capacity) {
        map_shared_memory(path, capacity, true);
    }

    ~ProducerNode() {
        unmap_shared_memory();
    }

    bool Send(uint32_t type, const void* data, uint32_t length) {
        size_t msg_total = sizeof(MessageHeader) + length;
        size_t cap = meta_->capacity;

        size_t curr_tail;
        do {
            curr_tail = meta_->tail.load(std::memory_order_relaxed);
            size_t curr_head = meta_->head.load(std::memory_order_acquire);
            if (curr_tail + msg_total - curr_head > cap) return false;
        } while (!meta_->tail.compare_exchange_weak(curr_tail, curr_tail + msg_total, std::memory_order_relaxed));

        MessageHeader hdr{type, length};
        ring_write(curr_tail, &hdr, sizeof(hdr));
        ring_write(curr_tail + sizeof(hdr), data, length);

        while (meta_->ready_tail.load(std::memory_order_acquire) != curr_tail);
        meta_->ready_tail.store(curr_tail + msg_total, std::memory_order_release);
        return true;
    }
};

class ConsumerNode : public BaseNode {
public:
    ConsumerNode(const char* path) {
        map_shared_memory(path, 0, false);
    }

    ~ConsumerNode() {
        unmap_shared_memory();
    }

    struct Message {
        uint32_t type;
        std::vector<uint8_t> payload;
    };

    std::optional<Message> Recv(uint32_t filter_type) {
        while (true) {
            size_t h = meta_->head.load(std::memory_order_relaxed);
            size_t rt = meta_->ready_tail.load(std::memory_order_acquire);

            if (h >= rt) return std::nullopt;

            MessageHeader hdr;
            ring_read(h, &hdr, sizeof(hdr));
            size_t msg_total = sizeof(hdr) + hdr.length;

            if (hdr.type == filter_type) {
                Message msg;
                msg.type = hdr.type;
                msg.payload.resize(hdr.length);
                ring_read(h + sizeof(hdr), msg.payload.data(), hdr.length);
                meta_->head.store(h + msg_total, std::memory_order_release);
                return msg;
            } else {
                meta_->head.store(h + msg_total, std::memory_order_release);
            }
        }
    }
};

}