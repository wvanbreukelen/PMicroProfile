#include "bench_suite.hpp"

#include <fcntl.h>
#include <unistd.h>

void BenchSuite::drop_caches()
{
    int fd;
	const char* data = "3";
	sync();

	fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
	write(fd, data, sizeof(char));
	close(fd);    
}

void BenchSuite::run()
{
    for (const auto &trace_entry : trace_file) {
        
    }

}