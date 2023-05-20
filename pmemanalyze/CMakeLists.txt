cmake_minimum_required(VERSION 3.10)

project(pmemanalyze LANGUAGES CXX)

find_package(Arrow REQUIRED QUIET)
find_package(Parquet REQUIRED QUIET)


# Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)

# Add compiler flags for Linux threading support
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -g -march=native -pthread")


# Add the main executable
add_executable(pmemanalyze src/trace.cpp src/bench_suite.cpp src/bench_export.cpp src/pmc.cpp src/worker.cpp main.cpp)


target_include_directories(pmemanalyze PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(pmemanalyze PRIVATE pthread Arrow::arrow_shared Parquet::parquet_shared)

install(TARGETS pmemanalyze DESTINATION /usr/local/bin)


add_custom_command(TARGET pmemanalyze POST_BUILD
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/plot/setup.py install || (exit 0)
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/plot
    COMMENT "Installing plotting tool..."
)


add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -E remove /usr/local/bin/pmemanalyze
)


