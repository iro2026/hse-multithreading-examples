#include "process_pool.hpp"
#include <chrono>
#include <iomanip>
#include <vector>

void* count_primes(void* arg) {
    int n = (int)(uintptr_t)arg; 
    
    int count = 0;
    for (int i = 2; i <= n; ++i) {
        bool is_prime = true;
        for (int j = 2; j * j <= i; ++j) {
            if (i % j == 0) { is_prime = false; break; }
        }
        if (is_prime) count++;
    }

    return (void*)(uintptr_t)count;
}

int main() {
    ProcessPool pool(4);
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<MyFuture<int>> futures;

    for (int i = 0; i < 16; ++i) {
        futures.push_back(pool.Submit<int, int>(count_primes_task, 2'000'000));
    }

    for (int i = 0; i < 16; ++i) {
        std::cout << "Task " << i << ": " << futures[i].get() << std::endl;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Total time: " << std::fixed << std::setprecision(3) 
              << std::chrono::duration<double>(end - start).count() << "s" << std::endl;
              
    return 0;
}
