#pragma once

#include <cstddef>

#include "trace.hpp"
#define MAX_SAMPLES 1000000


struct io_sample {
public:
    std::chrono::duration<long, std::nano> time_since_start;

    size_t num_reads = 0;
    size_t num_writes = 0;
    size_t num_flushes = 0;

    size_t num_classic_rw = 0;
    size_t num_movnti = 0;
    size_t num_movntq = 0;
    size_t num_movntqd = 0;
    size_t num_movntps = 0;

    size_t num_mfence = 0;
    size_t num_sfence = 0;
    size_t num_lfence = 0;
    
    size_t bytes_read = 0;
    size_t bytes_written = 0;
    size_t total_bytes_read_write = 0;

    unsigned long long read_inst_cycles = 0;
    unsigned long long write_inst_cycles = 0;
    unsigned long long flush_inst_cycles = 0;

    unsigned long long unc_ticks = 0;

    unsigned long long rpq_inserts = 0;
    unsigned long long wpq_inserts = 0;

    unsigned long long rpq_occupancy = 0;
    unsigned long long wpq_occupancy = 0;

    unsigned long long l3_misses_local_pmm = 0;
    unsigned long long l3_misses_remote_pmm = 0;

    unsigned long long pmm_any_snoop = 0;

    unsigned long prev_addr = 0;
    unsigned long total_addr_distance = 0;
};

template<typename Stream>
Stream& operator<<(Stream& os, const io_sample& sample) {
    os << sample.time_since_start.count() << ","
       << sample.num_reads << ","
       << sample.read_inst_cycles << ","
       << sample.num_writes << ","
       << sample.write_inst_cycles << ","
       << sample.num_flushes << ","
       << sample.flush_inst_cycles << ","
       << sample.bytes_read << ","
       << sample.bytes_written << ","

       << sample.num_classic_rw << ","
       << sample.num_movnti << ","
       << sample.num_movntq << ","
       << sample.num_movntqd << ","
       << sample.num_movntps << ","

       << sample.num_mfence << ","
       << sample.num_sfence << ","
       << sample.num_lfence << ","

       << sample.wpq_inserts << ","
       << sample.rpq_inserts << ","
       << sample.wpq_occupancy << ","
       << sample.rpq_occupancy << ","
       << sample.unc_ticks << ","
       << sample.total_bytes_read_write << ","

       << sample.l3_misses_local_pmm << ","
       << sample.l3_misses_remote_pmm << ","
       << sample.pmm_any_snoop << ","

       << sample.total_addr_distance;
    
    return os;
}

struct io_stat {
public:
    struct io_sample* samples = nullptr;
    size_t num_collected_samples = 0;

    size_t total_bytes = 0;
    size_t read_bytes = 0;
    size_t write_bytes = 0;
    int64_t latency_sum = 0;
};

struct WorkerArguments {
public:
    WorkerArguments(TraceFile *trace_file, const uint64_t num_samples, const size_t replay_rounds, const size_t cache_warming_rounds) :
        trace_file(trace_file),
        num_samples(num_samples),
        replay_rounds(replay_rounds),
        cache_warming_rounds(cache_warming_rounds)
    {}

    void init() {
        stat.samples = new io_sample[MAX_SAMPLES]();
    }

    WorkerArguments():
        trace_file(nullptr),
        num_samples(0),
        replay_rounds(0),
        cache_warming_rounds(0)
    {}

    ~WorkerArguments() {
        if (stat.samples)
            delete[] stat.samples;
    }

    TraceFile* trace_file;
    const size_t replay_rounds;
    const size_t cache_warming_rounds;
    const uint64_t num_samples;
    io_stat stat;
};
