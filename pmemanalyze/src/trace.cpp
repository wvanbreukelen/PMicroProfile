#include "trace.hpp"

#include <string>
#include <sstream>

#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>
#include <parquet/stream_reader.h>
#include <parquet/stream_writer.h>


#ifdef ENABLE_ASSERTS
#include <cassert>
#endif


bool parse_trace(const std::filesystem::path &path, TraceFile &trace)
{
    // https://arrow.apache.org/docs/cpp/parquet.html
    std::shared_ptr<arrow::io::ReadableFile> infile;
    try {
        PARQUET_ASSIGN_OR_THROW(
            infile,
            arrow::io::ReadableFile::Open(path));

    } catch(const parquet::ParquetInvalidOrCorruptedFileException& e) {
        std::cerr << "Unable to open trace file: " << e.what() << std::endl;
        return false;
    }

    parquet::StreamReader stream {parquet::ParquetFileReader::Open(infile)};

    TraceEntry trace_entry;
    uint32_t op;
    
    size_t count_reads = 0, count_writes = 0, count_flushes = 0;

    const size_t total_rows = stream.num_rows();
    size_t loaded_rows = 0;
    unsigned int percent_loaded = 0;

    trace.reserve(total_rows);

    while (!(stream.eof())) {
        stream >> trace_entry.timestamp_sec >> op >> trace_entry.opcode >> trace_entry.op_size >> trace_entry.abs_addr >> trace_entry.addr_offset >> trace_entry.data >> trace_entry.cpu_id >> trace_entry.syscall_nr >> parquet::EndRow;
        //std::cout << "OP: " << op << " opcode: 0x" << std::hex << opcode << " timestamp: " << timestamp_sec << " 0x" << std::hex << abs_addr << " data: 0x" << data << std::endl;
        trace_entry.op = static_cast<TraceOperation>(op);

        trace.push_back(trace_entry);

        // Calculate and print loading progress
        unsigned int new_percent_loaded = (static_cast<float>(loaded_rows++) / static_cast<float>(total_rows)) * 100;

        if (new_percent_loaded - percent_loaded >= 1 || loaded_rows == total_rows) {
            percent_loaded = new_percent_loaded;
            std::cout << "\rLoading trace, please wait (" << static_cast<int>(percent_loaded) << "%)";
            std::cout.flush();
        }

	//if (loaded_rows > 1000000)
	//	return true;
    }
    
    std::cout << std::endl;

    return true;
}

// std::optional<TraceFile> parse_trace(const std::string& filename)
// {
//     std::ifstream trace_handle(filename);

//     if (!trace_handle) {
//         std::cerr << "Failed to open trace file: " << filename << std::endl;
//         return std::nullopt;
//     }

//     std::string line;

//     if (!std::getline(trace_handle, line)) {
//         return std::nullopt;
//     }

//     std::regex re(R"(PMEMTRACE DEVICE:\s+\[(0x[\da-fA-F]+)-(0x[\da-fA-F]+)\])");
//     std::smatch match;

//     if (!std::regex_search(line, match, re)) {
//         std::cerr << "Invalid trace file format" << std::endl;
//         return std::nullopt;
//     }

//     const auto pmem_range_start = std::stoul(match[1].str(), nullptr, 16);
//     const auto pmem_range_end = std::stoul(match[2].str(), nullptr, 16);
//     std::cout << "Start address: " << std::hex << pmem_range_start << '\n';
//     std::cout << "End address: " << std::hex << pmem_range_end << '\n';
//     std::cout << "Size: " << std::dec << (pmem_range_end - pmem_range_start) / (1024 * 1024 * 1024) << " GiB" << std::endl; 
   
//     std::regex pattern(R"((R|W|F)\s+(\d+)\s+(0x[\da-fA-F]+)\s+([\d.]+)\s+\d+\s+(0x[\da-fA-F]+)\s+(0x[\da-fA-F]+))");
//     TraceFile trace;
//     std::smatch matches;
    
//     TraceOperation op;
//     while (std::getline(trace_handle, line)) {
//         if (std::regex_search(line, matches, pattern)) {
//             if (matches[1] == "R") {
//                 op = TraceOperation::READ;
//             } else if (matches[1] == "W") {
//                 op = TraceOperation::WRITE;
//             } else if (matches[1] == "F") {
//                 op = TraceOperation::CLFLUSH;
//             } else {
//                 std::cerr << "Unknown trace operation: " << matches[1] << std::endl;

//                 return std::nullopt;
//             }

//             // const std::vector<uint8_t> op_bytes = hex_string_to_bytes(matches[5]);
//             // std::cout << matches[5] << std::endl;

//             const unsigned long long data = std::stoull(matches[6], nullptr, 16);

//             //std::cout << std::hex << data << std::endl;

//             const unsigned long abs_addr = std::stoul(matches[5], nullptr, 16);
//             #ifdef ENABLE_ASSERTS
//             assert(abs_addr > pmem_range_start);
//             assert(abs_addr < pmem_range_end);
//             #endif
//             const unsigned long rel_addr = abs_addr - pmem_range_start;

//             //trace.emplace_back(op, std::stoi(matches[2]), std::stod(matches[3]), abs_addr, rel_addr, op_bytes);
//             trace.emplace_back(op, std::stoi(matches[2]), std::stoi(matches[3], nullptr, 16), std::stod(matches[4]), abs_addr, rel_addr, data);
//         }
//     }

//     return trace;
// }

