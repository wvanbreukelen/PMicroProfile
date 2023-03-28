#pragma once
#include "trace.hpp"

class BenchSuite {
public:
    BenchSuite(TraceFile &trace_file, const std::string& pmem_device_loc, const size_t mem_size, const unsigned int num_threads, const bool do_fallback_ram, const bool prealloc = true) :
        trace_file(trace_file),
        pmem_device_loc(pmem_device_loc),
        mem_size(mem_size),
        num_threads(num_threads),
        do_fallback_ram(do_fallback_ram)
    {
        mem_area = nullptr;
        dax_fd = -1;

        if (prealloc)
            allocate_mem_area();
    };

    ~BenchSuite()
    {
        deallocate_mem_area();
    }

    void run(const size_t replay_rounds);

private:
    void drop_caches();

    bool allocate_pmem_area();
    bool allocate_dram_area();

    void allocate_mem_area();
    void deallocate_mem_area();

    TraceFile &trace_file;
    const std::string pmem_device_loc;
    const size_t mem_size;
    const unsigned int num_threads;
    const bool do_fallback_ram;

    void* mem_area;
    int dax_fd;
};