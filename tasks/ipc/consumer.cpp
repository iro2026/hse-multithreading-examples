#include "queue.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    try {
        ipc::ConsumerNode consumer("/mpsc_ring_buffer");
        uint32_t target_type = 1;
        std::cout << "Consumer ready. Filtering type: " << target_type << std::endl;

        while (true) {
            auto msg = consumer.Recv(target_type);
            if (msg) {
                std::cout << "Received: " << reinterpret_cast<char*>(msg->payload.data()) << std::endl;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
