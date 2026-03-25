#include "process_pool.hpp"
#include <chrono>
#include <iomanip>
#include <vector>

struct StringTask {
    char text[64];
};

void count_primes_task(const void* arg_ptr, void* res_ptr) {
    int n = *(int*)arg_ptr;
    int count = 0;
    for (int i = 2; i <= n; ++i) {
        bool is_prime = true;
        for (int j = 2; j * j <= i; ++j) {
            if (i % j == 0) { is_prime = false; break; }
        }
        if (is_prime) count++;
    }
    std::memcpy(res_ptr, &count, sizeof(int));
}

void upper_case_task(const void* arg_ptr, void* res_ptr) {
    StringTask data = *(StringTask*)arg_ptr;
    for (int i = 0; data.text[i]; i++) {
        if (data.text[i] >= 'a' && data.text[i] <= 'z') 
            data.text[i] -= 32;
    }
    std::memcpy(res_ptr, &data, sizeof(StringTask));
}

int main() {
    ProcessPool pool(4);
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<MyFuture<int>> prime_futures;
    for (int i = 0; i < 16; ++i) {
        prime_futures.push_back(pool.Submit<int, int>(count_primes_task, 2'000'000));
    }

    StringTask st;
    std::strcpy(st.text, "hello from shm");
    auto string_future = pool.Submit<StringTask, StringTask>(upper_case_task, st);

    for (int i = 0; i < 16; ++i) {
        std::cout << "Primes " << i << ": " << prime_futures[i].get() << std::endl;
    }
    
    StringTask res_st = string_future.get();
    std::cout << "String result: " << res_st.text << std::endl;

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Time: " << std::fixed << std::setprecision(3) 
              << std::chrono::duration<double>(end - start).count() << "s" << std::endl;
              
    return 0;
}
