#include <iostream>
#include <filesystem>
#include <fstream>
#include "CLI11.hpp"
#include "benchmarks.hpp"

int main(int argc, char** argv)
{
    CLI::App app{"App description"};
    Benchmarks bm = Benchmarks(1);

    std::string pmem_path;
    size_t file_size = 0;
    size_t number_of_files = 0;
    size_t stride_size = 0;
    bool is_verbose = false;
    bool use_pmem_ops = false;

    app.add_option("-f,--path", pmem_path, "Path to PMEM (e.g. /mnt/pmem/bench)")->required();
    app.add_option("-s,--file-size", file_size, "File size")->default_val(4096 * 8);
    app.add_option("-n,--nr-files", number_of_files, "Number of files")->default_val(1);
    app.add_option("--stride-size", stride_size, "Stride size")->default_val(64);
    app.add_flag("-v,--verbose", is_verbose, "Enable verbose output")->default_val(false);
    app.add_flag("-p,--pmem-ops", use_pmem_ops, "Use pread/pwrite")->default_val(false);

    CLI11_PARSE(app, argc, argv);

    bm.set_stride_size(stride_size);

    if (pmem_path.rfind("/mnt", 0) != 0 && pmem_path.rfind("/tmp", 0) != 0) {
        std::cerr << "Path " << pmem_path << " is not a mount path, exiting..." << std::endl;
        return -1;
    }
    
    if (!std::filesystem::exists(pmem_path)) {
        std::cerr << "Mount path " << pmem_path << " not found, exiting..." << std::endl;
        return -1;
    }

    // for (size_t i = 0; i < number_of_files; ++i) {
    //     std::filesystem::path file_path{pmem_path + "/" + std::to_string(i)};

    //     if (is_verbose)
    //         std::cout << "Pre-propulating file: " << file_path << " of size " << std::to_string(file_size) << std::endl;

    //     //std::ofstream ofs(file_path);
    //     //std::filesystem::resize_file(file_path, file_size);
    //     //ofs.close();
    // }

    for (size_t i = 0; i < number_of_files; ++i) {
        std::filesystem::path file_path{pmem_path + "/" + std::to_string(i)};

	if (is_verbose)
		std::cout << "File: " << file_path << std::endl;

        bm.run_bench_file_seq(std::cout, file_path, IOOperation::WRITE, file_size, use_pmem_ops);
        bm.run_bench_file_seq(std::cout, file_path, IOOperation::READ, file_size, use_pmem_ops);
    }


    return 0;
}
