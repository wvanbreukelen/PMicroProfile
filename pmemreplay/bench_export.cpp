#include "bench_export.hpp"

#include <filesystem>
#include <iostream>
#include <fstream>

bool BenchExport::export_io_stat(const std::string &filename) const {

    std::filesystem::path export_path("../output/" + filename);
    std::filesystem::create_directories(export_path.parent_path());

    std::ofstream file_handle(export_path, std::ofstream::out| std::ofstream::trunc);

    if (!file_handle.is_open()) {
        std::cerr << "Unable to  export to file: " << export_path << std::endl;
        
        return false;
    }

    // Write CSV header.
    file_handle << "timestamp,num_reads,read_cycles,num_writes,write_cycles,num_flushes,flush_cycles,bytes_read,bytes_written,wpq_insert,rpq_inserts" << std::endl;

    const struct io_sample *samples = this->worker_args.stat.samples;

    for (size_t i = 0; i < this->worker_args.stat.num_collected_samples; ++i) {
        file_handle << samples[i].time_since_start.count() << "," <<
                    samples[i].num_reads << "," << samples[i].read_inst_cycles << "," <<
                    samples[i].num_writes << "," << samples[i].write_inst_cycles << "," <<
                    samples[i].num_flushes << "," << samples[i].flush_inst_cycles << "," <<
                    samples[i].bytes_read << "," << samples[i].bytes_written << "," <<
                    samples[i].wpq_inserts << "," << samples[i].rpq_inserts << std::endl;
    }
    
    file_handle.close();
    
    return true;
}