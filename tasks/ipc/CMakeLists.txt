cmake_minimum_required(VERSION 3.20)
project(MpscIpcQueue)

set(CMAKE_CXX_STANDARD 20)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(producer producer.cpp)
target_link_libraries(producer PRIVATE Threads::Threads rt)

add_executable(consumer consumer.cpp)
target_link_libraries(consumer PRIVATE Threads::Threads rt)
