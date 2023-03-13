#include "benchmarks.hpp"

#include "include/pm_util.hpp"

#define STRIDE_SIZE 64

void Benchmarks::run_bench_file_seq(std::ostream& os, std::filesystem::path path, IOOperation op, const size_t io_size)
{
    float imc_read, imc_write, media_read, media_write;
    size_t bytes_op = 0;

    FILE *fp = fopen(path.c_str(), "r");
    fseek(fp, 0, SEEK_SET);
    
    if (!fp)
        return;

    // unsigned char *dummy_data = malloc(io_size);
    unsigned char *dummy_data = (unsigned char*) malloc(STRIDE_SIZE);

    if (!dummy_data)
        return;

    for (size_t i = 0; i < STRIDE_SIZE; i++) {
        dummy_data[i] = rand();
    }
    
    switch (op)
    {
    case IOOperation::READ:
        {
            util::PmmDataCollector measure("PM data", &imc_read, &imc_write, &media_read, &media_write); 

            while (bytes_op < io_size) {
                fwrite(dummy_data, STRIDE_SIZE, 1, fp);
                bytes_op += STRIDE_SIZE;
            }
        }
        break;
    case IOOperation::WRITE:
        {
            fseek(fp, 0, SEEK_END);
            size_t real_file_size = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            if (io_size > real_file_size) {
                std::cerr << "Pre-allocated file too small!" << std::endl;
                
                goto out;
            }

            util::PmmDataCollector measure("PM data", &imc_read, &imc_write, &media_read, &media_write); 

            while (bytes_op < io_size) {
                fwrite(dummy_data, STRIDE_SIZE, 1, fp);
                bytes_op += STRIDE_SIZE;
            }
        }
        break;
    default:
        break;
    }

    // if (IOOperation::READ) {
    //     //std::cout << "Read Amplication: " << io_size / media_rd << std::endl;
    // } else if (IOOperation::WRITE) {
    //     //std::cout << "Write Amplication: " << io_size / media_wr << std::endl;
    // }

    std::cout << "[imc wr]:[" << imc_write
                      << "] [imc rd]:[" << imc_read
                      << "] [media wr]:[" << media_write
                      << "] [media rd]:[" << media_read << "]" << std::endl;
    

out:
    free(dummy_data);
    fclose(fp);
}

void Benchmarks::run_bench_file_strided(std::ostream& os, std::filesystem::path path, IOOperation op, const size_t io_size)
{
    float imc_rd, imc_wr, media_rd, media_wr;
    size_t num_written = 0;

    switch (op)
    {
    case IOOperation::READ:
        {
            util::PmmDataCollector measure("PM data", &imc_rd, &imc_wr, &media_rd, &media_wr); 

        
        }
        break;
    case IOOperation::WRITE:
        {
            util::PmmDataCollector measure("PM data", &imc_rd, &imc_wr, &media_rd, &media_wr); 

        }
        break;
    default:
        break;
    }
}
