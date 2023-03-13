#pragma once

#include <iostream>
#include <string>
#include <filesystem>

enum class IOOperation {
    READ,
    WRITE
};

class Benchmarks {
public:
    Benchmarks(unsigned int _num_cores) : num_cores(_num_cores) {};

    void run_bench_file_seq(std::ostream& os, std::filesystem::path path, IOOperation op, const size_t io_size);
    void run_bench_file_strided(std::ostream& os, std::filesystem::path path, IOOperation op, const size_t io_size);

    void set_num_cores(unsigned int _num_cores);
private:
    unsigned int num_cores;
};