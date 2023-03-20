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

static int open_file(const char* path)
{
    return open(path, O_RDONLY, S_IRUSR);
}

// https://gitlab.ethz.ch/comsec/spacefs/ffs/-/blob/master/test/test.c
static int create_file(const char* path)
{
	return open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
}

void Benchmarks::run_bench_file_seq(std::ostream& os, std::filesystem::path path, IOOperation op, const size_t io_size, const bool use_pmem_calls)
{
    float imc_read, imc_write, media_read, media_write;
    size_t bytes_op = 0;
    int fp;

    if (op == IOOperation::READ) {
        fp = open_file(path.c_str());
    } else {
        fp = create_file(path.c_str());
    }
    if (fp < 0) {
	    std::cerr << "Cannot open file!\n";
	    return;
	}

    // unsigned char *dummy_data = malloc(io_size);
    // unsigned char *dummy_data = (unsigned char*) malloc(this->stride_size);

    // if (!dummy_data)
    //     return;

    // for (size_t i = 0; i < this->stride_size; i++) {
    //     dummy_data[i] = rand();
    // }

    unsigned char *dummy_data = (unsigned char*) malloc(io_size);

    std::cout << "Stride size: " << std::to_string(this->stride_size) << " file size: " << std::to_string(io_size) << std::endl;

    drop_caches();    

    switch (op)
    {
        case IOOperation::READ:
        {
            size_t bytes_read;
            util::PmmDataCollector measure("PM data", &imc_read, &imc_write, &media_read, &media_write); 
            while (bytes_op < io_size) {

                bytes_read = (use_pmem_calls) ? pread(fp, dummy_data, this->stride_size, bytes_op) : read(fp, dummy_data, this->stride_size);

                if (bytes_read <= 0) {
                    std::cerr << "Read failed!" << std::endl;
                    goto out;
                }
		
                bytes_op += this->stride_size;
            }
        }
        break;
        case IOOperation::WRITE:
        {
            {
                ssize_t written;
                util::PmmDataCollector measure("PM data", &imc_read, &imc_write, &media_read, &media_write); 
                
		if (use_pmem_calls)
			std::cout << "Using pwrite calls..." << std::endl;

                while (bytes_op < io_size) {
                    written = (use_pmem_calls) ? pwrite(fp, dummy_data, this->stride_size, bytes_op) : write(fp, dummy_data, this->stride_size);

                    if (written <= 0) {
                        std::cerr << "Write failed!" << std::endl;
                        goto out;
                    }

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
    close(fp);
}
