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
    Benchmarks(const unsigned int _num_cores, const unsigned int _num_runs = 1) :
        num_cores(_num_cores),
        num_runs(_num_runs)
    {};

    void run_bench_file_seq(std::ostream& os, std::filesystem::path path, IOOperation op, const size_t io_size);
    void run_bench_file_strided(std::ostream& os, std::filesystem::path path, IOOperation op, const size_t io_size);

    void set_num_cores(const unsigned int _num_cores) {
        num_cores = _num_cores;
    }

    void set_stride_size(const size_t _stride_size) {
        stride_size = _stride_size;
    }

    void set_num_runs(const unsigned int _num_runs) {
        num_runs = _num_runs;
    }

private:
    unsigned int num_cores;
    unsigned int num_runs;
    size_t stride_size;
};