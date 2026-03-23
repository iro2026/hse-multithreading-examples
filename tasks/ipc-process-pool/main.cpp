#include "process_pool.hpp"
#include <chrono>
#include <iomanip>

int count_primes(int n) {
    int count = 0;
    for (int i = 2; i <= n; ++i) {
        bool is_prime = true;
        for (int j = 2; j * j <= i; ++j) {
            if (i % j == 0) { is_prime = false; break; }
        }
        if (is_prime) count++;
    }
    return count;
}

int main() {
    ProcessPool pool(4);
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<MyFuture> futures;

    for (int i = 0; i < 16; ++i) {
        futures.push_back(pool.Submit(count_primes, 2'000'000));
    }

    for (int i = 0; i < 16; ++i) {
        std::cout << "Task " << i << ": " << futures[i].get() << std::endl;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Time: " << std::fixed << std::setprecision(3) 
              << std::chrono::duration<double>(end - start).count() << "s" << std::endl;
    return 0;
}
