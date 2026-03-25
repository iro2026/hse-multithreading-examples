#include "process_pool.hpp"
#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <cstring>

const uint32_t TaskReverse = 1;
const uint32_t TaskMultiplyVector = 2;

std::vector<uint8_t> ReverseLogic(const std::vector<uint8_t>& input) {
    std::string s(input.begin(), input.end());
    std::reverse(s.begin(), s.end());
    return std::vector<uint8_t>(s.begin(), s.end());
}

std::vector<uint8_t> MultiplyLogic(const std::vector<uint8_t>& input) {
    if (input.empty()) return {};
    std::vector<int> nums(input.size() / sizeof(int));
    std::memcpy(nums.data(), input.data(), input.size());
    for (auto& n : nums) n *= 10;
    std::vector<uint8_t> res(nums.size() * sizeof(int));
    std::memcpy(res.data(), nums.data(), res.size());
    return res;
}

std::vector<uint8_t> MainProcessor(uint32_t type, const std::vector<uint8_t>& input) {
    if (type == TaskReverse)        return ReverseLogic(input);
    if (type == TaskMultiplyVector) return MultiplyLogic(input);
    return {};
}

int main() {
    ProcessPool pool(4, MainProcessor);

    std::string s = "Hello world";
    std::vector<uint8_t> d1(s.begin(), s.end());
    auto f1 = pool.Submit(TaskReverse, d1);

    std::vector<int> n = {10, 20, 30};
    std::vector<uint8_t> d2(n.size() * sizeof(int));
    std::memcpy(d2.data(), n.data(), d2.size());
    auto f2 = pool.Submit(TaskMultiplyVector, d2);

    auto r1 = f1->Get();
    std::cout << "String: " << std::string(r1.begin(), r1.end()) << std::endl;

    auto r2 = f2->Get();
    std::vector<int> res_n(r2.size() / sizeof(int));
    std::memcpy(res_n.data(), r2.data(), r2.size());
    std::cout << "Numbers: ";
    for(int x : res_n) std::cout << x << " ";
    std::cout << std::endl;

    return 0;
}
