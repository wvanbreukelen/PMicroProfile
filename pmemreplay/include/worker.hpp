#pragma once

#include "trace.hpp"
#define MAX_SAMPLES 1000000


struct io_sample {
public:
    std::chrono::duration<long, std::nano> time_since_start;

    size_t num_reads = 0;
    size_t num_writes = 0;
    size_t num_flushes = 0;
    
    size_t bytes_read = 0;
    size_t bytes_written = 0;

    unsigned long long read_inst_cycles = 0;
    unsigned long long write_inst_cycles = 0;
    unsigned long long flush_inst_cycles = 0;

    unsigned long long unc_ticks = 0;

    unsigned long long rpq_inserts = 0;
    unsigned long long wpq_inserts = 0;

    unsigned long long rpq_occupancy = 0;
    unsigned long long wpq_occupancy = 0;
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
       << sample.wpq_inserts << ","
       << sample.rpq_inserts << ","
       << sample.wpq_occupancy << ","
       << sample.rpq_occupancy << ","
       << sample.unc_ticks;
    
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
    WorkerArguments(TraceFile *trace_file, const size_t replay_rounds) :
        trace_file(trace_file),
        replay_rounds(replay_rounds)
    {}

    void init() {
        stat.samples = new io_sample[MAX_SAMPLES]();
    }

    WorkerArguments():
        trace_file(nullptr),
        replay_rounds(0)
    {}

    ~WorkerArguments() {
        if (stat.samples)
            delete[] stat.samples;
    }

    TraceFile* trace_file;
    const size_t replay_rounds;
    io_stat stat;
};
