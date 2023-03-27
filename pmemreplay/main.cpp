#include <optional>

#include "CLI11.hpp"
#include "trace.hpp"
#include "bench_suite.hpp"


int main(int argc, char** argv)
{
    CLI::App app{"App description"};

    bool is_verbose;
    unsigned int num_threads;
    std::string trace_file;

    app.add_option("trace file", trace_file, "Trace file to execute (must have .trf extension)")
        ->required()
        ->check([](std::string input){
            if (input.size() < 4 || input.substr(input.size() - 4) != ".trf") {
                return "Trace file must have .trf extension";
            }
            return "";
        });
    app.add_flag("-v,--verbose", is_verbose, "Enable verbose output")
        ->default_val(false);
    app.add_option("-t, --num-threads", num_threads, "Number of execution threads")
        ->default_val(1);

    CLI11_PARSE(app, argc, argv);

    std::optional<TraceFile> trace = parse_trace(trace_file);

    if (!trace) {
        std::cout << "Unable to parse trace " << trace_file << ", exiting..." << std::endl;
        return 1;
    }

    //std::regex pattern(R"(([RW])\s+([\d]+)+\s+([\d.]+)\s+\d+\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s+.*)");
   

    std::cout << std::dec;
    std::cout << "Total bytes read: " << trace->get_total(TraceOperation::READ) << " total bytes write: " << trace->get_total(TraceOperation::WRITE) << std::endl;

    return 0;
}
