#pragma once

#include "trace.hpp"

class BenchSuite {
public:
    BenchSuite(const TraceFile &trace_file, const size_t mem_size, const unsigned int num_threads) :
        trace_file(trace_file),
        mem_size(mem_size),
        num_threads(num_threads)
    {
        mem_area = nullptr;
        dax_fd = -1;
    };

    void run();

private:
    void drop_caches();
    bool allocate_pmem_area();
    bool allocate_dram_area();
    void deallocate_mem_area();

    const TraceFile &trace_file;
    const size_t mem_size;
    const unsigned int num_threads;

    void* mem_area;
    int dax_fd;
};