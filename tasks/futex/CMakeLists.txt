cmake_minimum_required(VERSION 3.16)
project(FutexMutexTask LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(GTest REQUIRED)

add_executable(run_tests test.cpp)

target_link_libraries(run_tests PRIVATE 
    Threads::Threads 
    GTest::gtest 
    GTest::gtest_main
)

target_compile_options(run_tests PRIVATE -fsanitize=thread -O3 -g)
target_link_options(run_tests PRIVATE -fsanitize=thread)

enable_testing()
add_test(NAME MutexUnitTests COMMAND run_tests)
