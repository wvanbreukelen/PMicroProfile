cmake_minimum_required(VERSION 3.1)

project(my_project)

# Set the C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)

# Find the pthread library
find_package(Threads REQUIRED)

# Add the main executable
add_executable(my_executable main.cpp include/utils.cpp benchmarks.cpp)

# Link against pthread
target_link_libraries(my_executable Threads::Threads)
