#include "queue.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

int main() {
    try {
        ipc::ProducerNode producer("/mpsc_ring_buffer", 4096);
        std::string input;
        uint32_t type_counter = 0;

        std::cout << "Producer ready. Enter messages:" << std::endl;
        while (std::getline(std::cin, input)) {
            uint32_t type = (type_counter++ % 2) + 1;
            if (producer.Send(type, input.c_str(), input.size() + 1)) {
                std::cout << "Sent type " << type << ": " << input << std::endl;
            } else {
                std::cerr << "Queue full" << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
