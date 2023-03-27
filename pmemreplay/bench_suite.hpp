#pragma once

#include "trace.hpp"

class BenchSuite {
public:
    BenchSuite(const TraceFile &trace_file) :
        trace_file(trace_file)
    {};

    void run();

private:
    void drop_caches();

    const TraceFile &trace_file;
};