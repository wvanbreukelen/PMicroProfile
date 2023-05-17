#pragma once

#include "worker.hpp"

class BenchExport
{
public:
    BenchExport(const WorkerArguments &worker_args) :
        worker_args(worker_args)
    {}

    bool export_io_stat(const std::string& filename) const;

private:
    const WorkerArguments &worker_args;
};