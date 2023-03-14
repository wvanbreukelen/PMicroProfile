#include "benchmarks.hpp"
#include <fcntl.h>
#include <cmath>
#include <unistd.h>
//	#include "include/pm_util.hpp"
#include "pm_util.h"


static void drop_caches()
{
	int fd;
	const char* data = "3";
	sync();

	fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
	write(fd, data, sizeof(char));
	close(fd);
}

void Benchmarks::run_bench_file_seq(std::ostream& os, std::filesystem::path path, IOOperation op, const size_t io_size)
{
    float imc_read, imc_write, media_read, media_write;
    size_t bytes_op = 0;

    FILE *fp = fopen(path.c_str(), ((op == IOOperation::WRITE) ? "w" : "r"));
    
    if (!fp) {
	std::cerr << "Cannot open file!\n";
	return;
	}
    fseek(fp, 0, SEEK_SET);

    // unsigned char *dummy_data = malloc(io_size);
    unsigned char *dummy_data = (unsigned char*) malloc(this->stride_size);

    if (!dummy_data)
        return;

    for (size_t i = 0; i < this->stride_size; i++) {
        dummy_data[i] = rand();
    }

    std::cout << "Stride size: " << std::to_string(this->stride_size) << " file size: " << std::to_string(io_size) << std::endl;

    drop_caches();    

    switch (op)
    {
    case IOOperation::READ:
        {
            util::PmmDataCollector measure("PM data", &imc_read, &imc_write, &media_read, &media_write); 
            while (bytes_op < io_size) {
		//std::cout << "Here!" << std::endl;
                if (fread(dummy_data, this->stride_size, sizeof(char), fp) < 0) {
                    std::cerr << "Read failed!" << std::endl;
                    goto out;
                }

		fflush(fp);

                bytes_op += this->stride_size;
		//std::cout << bytes_op << " " << dummy_data[0] << std::endl;
            }
        }
        break;
    case IOOperation::WRITE:
        {
            fseek(fp, 0, SEEK_END);
            size_t real_file_size = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            if (io_size > real_file_size) {
                std::cerr << "Pre-allocated file " << path << " is too small! File is " << real_file_size << " but should be " << io_size << " bytes" << std::endl;
                
                //goto out;
            }
	{

            util::PmmDataCollector measure("PM data", &imc_read, &imc_write, &media_read, &media_write); 

            while (bytes_op < io_size) {
                if (fwrite(dummy_data, this->stride_size, sizeof(char), fp) < 0) {
                    std::cerr << "Write failed!" << std::endl;
                    
                    goto out;
                }

                fflush(fp);

                bytes_op += this->stride_size;
            }
	}

        }
        break;
    default:
        break;
    }

    std::cout.precision(3);

    if (op == IOOperation::READ) {
         std::cout << "Read Amplication: " << (media_read / imc_read) << std::endl;
    } else if (op == IOOperation::WRITE) {
         std::cout << "Write Amplication: " << (media_write / imc_write) << std::endl;
    }

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
