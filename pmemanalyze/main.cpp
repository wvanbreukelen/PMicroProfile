#include <optional>
#include <string>
#include <cstdlib>

#include "CLI11.hpp"
#include "trace.hpp"
#include "bench_suite.hpp"

//#define BENCH_MAP_SIZE (28UL * 1024 * 1024 * 1024)

std::string run_plot_cmd(const std::string &cmd);

int main(int argc, char** argv)
{
    CLI::App app{"pmemanalyze - A Persistent Memory Trace Replaying Tool"};

    bool is_verbose = false;
    const unsigned int num_threads = 1;
    std::string trace_file = "trace.parquet";
    std::string pmem_device_loc;
    std::string plot_path;
    size_t replay_rounds;
    bool do_fallback_ram = false;
    bool force_dram = false;
    bool do_cache_warming = false;
    bool do_plot = false;
    const uint64_t num_samples = 2000;
    size_t bench_map_size_gib = 28;


    auto trace_file_option = app.add_option("trace file", trace_file, "Trace file to execute (must have .parquet extension)")
        ->check([](std::string input){
            if (input.size() < 8 || input.substr(input.size() - 8) != ".parquet") {
                return "Trace file must have .parquet extension";
            }

            return "";
        });
    // app.add_option("-t, --num-threads", num_threads, "Number of execution threads.")
    //     ->default_val(1);
    app.add_option("-r, --replay-rounds", replay_rounds, "Number of replay rounds.")
        ->default_val(0);
    app.add_option("-s, --dax-size", bench_map_size_gib, "DAX mapping size, should match the PMEM device size of the trace origin machine. The default setting (28 GiB) should be sufficient in most cases.")
        ->default_val(28)->expected(1, 256);
    app.add_flag("-v,--verbose", is_verbose, "Enable verbose output.")
        ->default_val(false);
    app.add_flag("--fallback-ram", do_fallback_ram, "Fallback to RAM in case Persistent Memory is not available.")
        ->default_val(false);
    app.add_flag("--cache-warming", do_cache_warming, "Enable cache warming.")
        ->default_val(false);
    app.add_option("d, --device", pmem_device_loc, "Location of Persistent Memory DAX device. Enter 'dram' to use DRAM instead.")
        ->default_val("/dev/dax0.0");
    
    app.add_flag("--plot", do_plot, "Set this flag to plot results.")
        ->default_val(false);
    app.add_option("--plot-file", plot_path, "Set the file to plot (default: out.csv)")
        ->default_val("out.csv")->excludes(trace_file_option);
    // app.add_option("-s,--num-samples", num_samples, "Number of samples to collect.")
    //     ->default_val(2000);

    CLI11_PARSE(app, argc, argv);

    if (do_plot) {
        if (std::filesystem::exists(plot_path)) {
            std::string cmd = "create_figures.py ";
            run_plot_cmd(cmd + plot_path);
        } else {
            std::cerr << "Cannot find " << plot_path << " file. Please specify manually using --plot-file" << std::endl;
        }

        return 0;
    }


    if (!std::filesystem::exists(trace_file)) {
        std::cerr << "Trace file " + trace_file + " does not exists, please specify trace file, e.g. pmemanalyze trace.parquet" << std::endl;
        return -1;
    }

    std::transform(pmem_device_loc.begin(), pmem_device_loc.end(), pmem_device_loc.begin(), ::tolower);

    if (pmem_device_loc == "dram") {
        std::cout << "Using DRAM as backing device..." << std::endl;
        force_dram = true;
    }

    TraceFile trace;

    if (!(parse_trace(trace_file, trace))) {
        std::cout << "Unable to parse trace " << trace_file << ", exiting..." << std::endl;
        return 1;
    }

    std::cout << "Trace: #reads: " << trace.get_total(TraceOperation::READ) << " #writes: " << trace.get_total(TraceOperation::WRITE) << " #flushes: " << trace.get_total(TraceOperation::CLFLUSH);
    std::cout << " (size: " << (trace.size() * sizeof(TraceEntry)) / (1024 * 1024) << " MiB)" << std::endl;

    BenchSuite bsuite(trace, pmem_device_loc, (bench_map_size_gib * 1024 * 1024 * 1024), num_threads, num_samples, force_dram, do_fallback_ram, do_cache_warming);

    if (!bsuite.run(replay_rounds)) {
        std::cerr << "Error occured, exiting..." << std::endl;
        return 1;
    }

    // std::cout << std::dec;
    // std::cout << "Total bytes read: " << trace->get_total(TraceOperation::READ) << " total bytes write: " << trace->get_total(TraceOperation::WRITE) << std::endl;

    return 0;
}

std::string run_plot_cmd(const std::string& cmd)
{
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        result += buffer.data();

    return result;
}