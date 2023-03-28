#include <optional>
#include <string>

#include "CLI11.hpp"
#include "trace.hpp"
#include "bench_suite.hpp"

#define BENCH_MAP_SIZE (30UL * 1024 * 1024 * 1024)

int main(int argc, char** argv)
{
    CLI::App app{"App description"};

    bool is_verbose = false;
    unsigned int num_threads;
    std::string trace_file;
    std::string pmem_device_loc;
    size_t replay_rounds;
    bool do_fallback_ram = false;
    bool force_dram = false;

    app.add_option("trace file", trace_file, "Trace file to execute (must have .trf extension)")
        ->required()
        ->check([](std::string input){
            if (input.size() < 4 || input.substr(input.size() - 4) != ".trf") {
                return "Trace file must have .trf extension";
            }
            return "";
        });
    app.add_option("-t, --num-threads", num_threads, "Number of execution threads")
        ->default_val(1);
    app.add_option("-r, --replay-rounds", replay_rounds, "Number of replay rounds")
        ->default_val(0);
    app.add_flag("-v,--verbose", is_verbose, "Enable verbose output")
        ->default_val(false);
    app.add_flag("--fallback-ram", do_fallback_ram, "Fallback using RAM in case Persistent Memory is not available")
        ->default_val(false);
    app.add_option("d, --device", pmem_device_loc, "Location of Persistent Memory DAX device. Enter 'dram' to use DRAM instead.")
        ->default_val("/dev/dax0.0");

    CLI11_PARSE(app, argc, argv);

    std::transform(pmem_device_loc.begin(), pmem_device_loc.end(), pmem_device_loc.begin(), ::tolower);

    if (pmem_device_loc == "dram") {
        std::cout << "Using DRAM as backing device..." << std::endl;
        force_dram = true;
    }

    std::optional<TraceFile> trace = parse_trace(trace_file);

    if (!trace) {
        std::cout << "Unable to parse trace " << trace_file << ", exiting..." << std::endl;
        return 1;
    }

    BenchSuite bsuite(*trace, pmem_device_loc, BENCH_MAP_SIZE, num_threads, force_dram, do_fallback_ram);

    bsuite.run(replay_rounds);

    // std::cout << std::dec;
    // std::cout << "Total bytes read: " << trace->get_total(TraceOperation::READ) << " total bytes write: " << trace->get_total(TraceOperation::WRITE) << std::endl;

    return 0;
}
