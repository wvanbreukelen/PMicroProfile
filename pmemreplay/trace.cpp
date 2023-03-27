#include "trace.hpp"

std::ostream& operator<<(std::ostream &os, const TraceEntry &entry) {
    os << "[" << entry.timestamp_sec << "] OP: " << ((entry.op == TraceOperation::READ) ? "read" : "write") << " OP size: " << entry.op_size << " addr: " << "0x" << std::hex << entry.addr;

    return os;
}

std::optional<TraceFile> parse_trace(const std::string& filename)
{
    std::ifstream trace_handle(filename);

    if (!trace_handle) {
        std::cerr << "Failed to open trace file: " << filename << std::endl;
        return std::nullopt;
    }

    std::regex pattern(R"((R|W)\s+(\d+)\s+([\d.]+)\s+\d+\s+(0x[\da-fA-F]+)\s+(0x[\da-fA-F]+))");
    std::smatch matches;
    TraceFile trace;
    
    std::string line;
    TraceOperation op;
    while (std::getline(trace_handle, line)) {
        if (std::regex_search(line, matches, pattern)) {
            if (matches[1] == "R") {
                op = TraceOperation::READ;
            } else if (matches[1] == "W") {
                op = TraceOperation::WRITE;
            } else {
                std::cerr << "Unknown trace operation: " << matches[1] << std::endl;

                return std::nullopt;
            }

            trace.emplace_back(op, std::stoi(matches[2]), std::stod(matches[3]), std::stoul(matches[4], nullptr, 16));
        }
    }

    return trace;
}

