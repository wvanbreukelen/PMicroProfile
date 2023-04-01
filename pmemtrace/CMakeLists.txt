cmake_minimum_required(VERSION 3.5)
project(pmemtrace)

find_package(Arrow REQUIRED QUIET)
find_package(Parquet REQUIRED QUIET)


# set(CMAKE_C_STANDARD 11)
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall -Wextra -pthread")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -Wall -Wextra -pthread -Wno-unused-result")

add_executable(pmemtrace pmemtrace.cpp)
add_executable(tracecompress tracecompress.cpp)
target_link_libraries(pmemtrace PRIVATE pthread Arrow::arrow_shared Parquet::parquet_shared)
target_link_libraries(tracecompress PRIVATE Arrow::arrow_shared Parquet::parquet_shared)

