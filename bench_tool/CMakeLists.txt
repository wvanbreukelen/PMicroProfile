cmake_minimum_required(VERSION 3.1)

#set(pm_util_dir ${PROJECT_SOURCE_DIR}/../pm_util)

project(my_project)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(pm_util_dir ${PROJECT_SOURCE_DIR}/../pm_util)

# Set the C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)

# Find the pthread library
#find_package(Threads REQUIRED)

add_subdirectory(${PROJECT_SOURCE_DIR}/pm_util/ build)

include_directories(
    ${PROJECT_SOURCE_DIR}/pm_util/include
)

set(CMAKE_CXX_FLAGS "-march=native")
add_definitions(-w)

# Add the main executable
add_executable(my_executable main.cpp benchmarks.cpp)

# Link against pthread
target_link_libraries(my_executable pm_util pthread numa)

