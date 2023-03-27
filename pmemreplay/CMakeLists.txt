cmake_minimum_required(VERSION 3.10)

project(pmemreplay)

# Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)

# Add compiler flags for Linux threading support
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

# Add the main executable
add_executable(pmemreplay trace.cpp bench_suite.cpp main.cpp)
