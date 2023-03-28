#include "trace.hpp"

#include <string>

#include <sstream>
#include <cassert>

std::ostream& operator<<(std::ostream &os, const TraceEntry &entry) {
    os << "[" << entry.timestamp_sec << "] OP: " << ((entry.op == TraceOperation::READ) ? "read" : "write") << " OP size: " << entry.op_size << " addr: "
        << "0x" << std::hex << entry.abs_addr
        << " data: 0x" << entry.data
        << " opcode: 0x" << entry.opcode;

    return os;
}

const std::vector<uint8_t> hex_string_to_bytes(const std::string& hexString) {
    std::vector<uint8_t> bytes;

    // Make sure the string starts with "0x"
    if (hexString.substr(0, 2) != "0x") {
        throw std::invalid_argument("Input string must start with '0x'");
    }

    std::string hex = hexString.substr(2);

    // If the string has an odd number of characters, add a leading zero
    if (hex.length() % 2 == 1) {
        hex = "0" + hex;
    }

    std::vector<uint8_t> result;

    // Iterate over the string, converting each pair of characters to a byte
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        uint8_t byte = std::stoi(byteString, nullptr, 16);
        assert(byte < 256);
        result.push_back(byte);
    }

    return result;
}

std::optional<TraceFile> parse_trace(const std::string& filename)
{
    std::ifstream trace_handle(filename);

    if (!trace_handle) {
        std::cerr << "Failed to open trace file: " << filename << std::endl;
        return std::nullopt;
    }

    std::string line;

    if (!std::getline(trace_handle, line)) {
        return std::nullopt;
    }

    std::regex re(R"(PMEMTRACE DEVICE:\s+\[(0x[\da-fA-F]+)-(0x[\da-fA-F]+)\])");
    std::smatch match;

    if (!std::regex_search(line, match, re)) {
        std::cerr << "Invalid trace file format" << std::endl;
        return std::nullopt;
    }

    const auto pmem_range_start = std::stoul(match[1].str(), nullptr, 16);
    const auto pmem_range_end = std::stoul(match[2].str(), nullptr, 16);
    std::cout << "Start address: " << std::hex << pmem_range_start << '\n';
    std::cout << "End address: " << std::hex << pmem_range_end << '\n';
    std::cout << "Size: " << std::dec << (pmem_range_end - pmem_range_start) / (1024 * 1024 * 1024) << " GiB" << std::endl; 
   
    std::regex pattern(R"((R|W|F)\s+(\d+)\s+(0x[\da-fA-F]+)\s+([\d.]+)\s+\d+\s+(0x[\da-fA-F]+)\s+(0x[\da-fA-F]+))");
    TraceFile trace;
    std::smatch matches;
    
    TraceOperation op;
    while (std::getline(trace_handle, line)) {
        if (std::regex_search(line, matches, pattern)) {
            if (matches[1] == "R") {
                op = TraceOperation::READ;
            } else if (matches[1] == "W") {
                op = TraceOperation::WRITE;
            } else if (matches[1] == "F") {
                op = TraceOperation::CLFLUSH;
            } else {
                std::cerr << "Unknown trace operation: " << matches[1] << std::endl;

                return std::nullopt;
            }

            // const std::vector<uint8_t> op_bytes = hex_string_to_bytes(matches[5]);
            // std::cout << matches[5] << std::endl;

            const unsigned long long data = std::stoull(matches[6], nullptr, 16);

            //std::cout << std::hex << data << std::endl;

            const unsigned long abs_addr = std::stoul(matches[5], nullptr, 16);
            assert(abs_addr > pmem_range_start);
            assert(abs_addr < pmem_range_end);
            const unsigned long rel_addr = abs_addr - pmem_range_start;

            //trace.emplace_back(op, std::stoi(matches[2]), std::stod(matches[3]), abs_addr, rel_addr, op_bytes);
            trace.emplace_back(op, std::stoi(matches[2]), std::stoi(matches[3], nullptr, 16), std::stod(matches[4]), abs_addr, rel_addr, data);
        }
    }

    return trace;
}

