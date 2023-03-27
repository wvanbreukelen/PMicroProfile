cmake_minimum_required(VERSION 3.10)

project(pmemreplay LANGUAGES CXX)

# Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)

# Add compiler flags for Linux threading support
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -O3 -mavx -mavx2 -march=native")

# Add the main executable
add_executable(pmemreplay trace.cpp bench_suite.cpp main.cpp)

target_include_directories(pmemreplay PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(pmemreplay PRIVATE pthread)
