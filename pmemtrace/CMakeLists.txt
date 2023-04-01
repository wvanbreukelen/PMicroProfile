cmake_minimum_required(VERSION 3.5)
project(pmemtrace)

find_package(Arrow REQUIRED)
find_package(Parquet REQUIRED)


set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall -Wextra -pthread")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -pthread")

add_executable(pmemtrace pmemtrace.c)
add_executable(tracecompress tracecompress.cpp)
target_link_libraries(tracecompress PRIVATE Arrow::arrow_shared Parquet::parquet_shared)

