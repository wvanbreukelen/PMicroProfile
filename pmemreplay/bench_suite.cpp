#include "bench_suite.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <cassert>
#include <chrono>

#include <immintrin.h>
#include <xmmintrin.h>

#define Mebibyte (1024 * 1024)
#define Gibibyte (1024 * 1024 * 1024)

#define ALIGN_TO_64(ptr) ((char*)(((uintptr_t)(ptr) + 63) & ~(uintptr_t)63))
// #define FORCE_FLUSHING

static constexpr size_t CACHE_LINE_SIZE = 64;

struct io_stat {
public:
    size_t total_bytes = 0;
    size_t read_bytes = 0;
    size_t write_bytes = 0;
    int64_t latency_sum = 0;
};

struct WorkerArguments {
public:
    WorkerArguments(TraceFile *trace_file, const size_t replay_rounds) :
        trace_file(trace_file),
        replay_rounds(replay_rounds)
    {}

    WorkerArguments():
        trace_file(nullptr),
        replay_rounds(0)
    {}

    TraceFile* trace_file;
    const size_t replay_rounds;
    io_stat stat;
};


// from: https://github.com/hpides/perma-bench/blob/75b6e3ceea6895fdb779b4981aa43a2ff6185104/src/read_write_ops.hpp
#define READ_SIMD_512(mem_addr, offset) _mm512_load_si512((void*)((mem_addr) + ((offset)*CACHE_LINE_SIZE)))

#define WRITE_SIMD_NT_512(mem_addr, offset, data) \
  _mm512_stream_si512(reinterpret_cast<__m512i*>((mem_addr) + ((offset)*CACHE_LINE_SIZE)), data)

#define WRITE_SIMD_512(mem_addr, offset, data) \
  _mm512_store_si512(reinterpret_cast<__m512i*>((mem_addr) + ((offset)*CACHE_LINE_SIZE)), data)

#define READ_SIMD_256(mem_addr, offset) _mm256_load_si256((void*)((mem_addr) + ((offset)*CACHE_LINE_SIZE)))

#define WRITE_SIMD_NT_256(mem_addr, offset, data) \
  _mm256_stream_si256(reinterpret_cast<__m256i*>((mem_addr) + ((offset)*CACHE_LINE_SIZE)), data)

#define WRITE_SIMD_256(mem_addr, offset, data) \
  _mm256_store_si256(reinterpret_cast<__m256i*>((mem_addr) + ((offset)*CACHE_LINE_SIZE)), data)


inline void flush_clwb(char* addr, const size_t len) {
    const char* end_addr = addr + len;
    for (char* current_cl = addr; current_cl < end_addr; current_cl += CACHE_LINE_SIZE) {
        _mm_clwb(current_cl);
    }
}

inline void flush_clflushopt(char* addr, const size_t len) {
    const char* end_addr = addr + len;
    for (char* current_cl = addr; current_cl < end_addr; current_cl += CACHE_LINE_SIZE) {
        _mm_clflushopt(current_cl);
    }
}

static inline void dax_write(char* addr, uint8_t* data, const size_t data_len) {
  // Write 512 Bit (64 Byte)
//   __m512i* data = (__m512i*)(WRITE_DATA);
//   WRITE_SIMD_512(addr, 0, *data);
    memcpy(addr, data, data_len);

    flush_clflushopt(addr, data_len);
    //barrier();
    _mm_sfence();
}


void BenchSuite::drop_caches()
{
    int fd;
	const char* data = "3";
	sync();

	fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
    if (!fd) {
        std::cerr << "Warning: unable to drop caches! Are you running without sudo?" << std::endl;
        return;
    }

	if (write(fd, data, 1) < 1) {
        std::cerr << "Warning: unable to drop caches! Are you running without sudo?" << std::endl;
    }
	close(fd);    
}

bool BenchSuite::allocate_pmem_area()
{
    int fd = open(this->pmem_device_loc.c_str(), O_RDWR);

    if (fd < 0) {
        return false;
    }

    void *dax_area = mmap(NULL, this->mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (dax_area == MAP_FAILED) {
        return false;
    }

    // if (mlock(dax_area, this->mem_size) < 0) {
    //     std::cerr << "Warning: unable to mlock DAX-backed memory region" << std::endl;
    // }

    this->mem_area = dax_area;
    this->dax_fd = fd;

    return true;
}

bool BenchSuite::allocate_dram_area()
{
    void *mem_area = mmap(NULL, this->mem_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if (mem_area == MAP_FAILED) {
        return false;
    }

    if (mprotect(mem_area, this->mem_size, PROT_READ | PROT_WRITE) < 0) {
        std::cerr << "Warning: unable to mprotect DRAM-backed memory region" << std::endl;
    }

    if (mlock(mem_area, this->mem_size) < 0) {
        std::cerr << "Warning: unable to mlock DRAM-backed memory region" << std::endl;
    }


    this->mem_area = mem_area;

    return true;
}

void BenchSuite::allocate_mem_area()
{
    if (force_ram) {
        if (!allocate_dram_area()) {
            std::cerr << "Unable to allocate DRAM-backed region, exiting..." << std::endl;
            return;
        }

        return;
    }

    if (!allocate_pmem_area()) {
        std::cerr << "Unable to allocate DAX-backed region at " << this->pmem_device_loc << ", switching to main memory backed area..." << std::endl;

        if (!this->do_fallback_ram) {
            std::cerr << "Exiting. Fallback to RAM can be enabled by setting the --fallback-ram flag." << std::endl;
            exit(1);
            return;
        }

        if (!allocate_dram_area()) {
            std::cerr << "Unable to allocate DRAM-backed region, exiting..." << std::endl;
            return;
        }
    }
}

void BenchSuite::deallocate_mem_area()
{
    if (this->mem_area) {
        munlock(this->mem_area, this->mem_size);
        munmap(this->mem_area, this->mem_size);
    }

    if (this->dax_fd >= 0) {
        close(this->dax_fd);
        this->dax_fd = -1;
    }
}

/**
static void clean_cache_range(void *addr, size_t size)
{
	u16 x86_clflush_size = boot_cpu_data.x86_clflush_size;
	unsigned long clflush_mask = x86_clflush_size - 1;
	void *vend = addr + size;
	void *p;

	for (p = (void *)((unsigned long)addr & ~clflush_mask);
	     p < vend; p += x86_clflush_size)
		clwb(p);
}
*/


static void* do_work(void *arg)
{
    struct WorkerArguments *args = static_cast<struct WorkerArguments*>(arg);
    char* dev_addr = nullptr;

    size_t count = 0;
    volatile unsigned long long temp_var = 0;
    
    const auto time_start = std::chrono::high_resolution_clock::now();
    size_t total_bytes = 0;
    size_t i = 0;
    // TraceOperation prev_op = TraceOperation::UNKNOWN;
    // char *prev_addr = nullptr;
    // size_t stride_write_size = 0;



    for (; i < args->replay_rounds + 1; ++i) {
        //prev_op = (*args->trace_file)[0].op;

        for (const TraceEntry& entry : *(args->trace_file)) {
            dev_addr = static_cast<char*>(entry.dax_addr);
            if (entry.op == TraceOperation::READ) {
                switch (entry.op_size)
                {
                    case 1:
                        *(reinterpret_cast<volatile char*>(&temp_var)) = *(dev_addr);

                        break;
                    case 4:
                        *(reinterpret_cast<volatile int*>(&temp_var)) = *(dev_addr);

                        break;
                    case 8:
                        *(reinterpret_cast<volatile long*>(&temp_var)) = *(dev_addr);
                        break;
                    default:
                        std::cerr << "Unsupported op size " << entry.op_size << "!" << std::endl;

                        pthread_exit(NULL);
                        break;
                }

                total_bytes += entry.op_size;
            } else if (entry.op == TraceOperation::WRITE) {
                switch (entry.op_size)
                {
                case 1:
                    *(dev_addr) = static_cast<unsigned char>(entry.data);

                    #ifdef FORCE_FLUSHING
                    _mm_clflushopt(ALIGN_TO_64(static_cast<const char*>(dev_addr)));
                    _mm_sfence();
                    #endif
                    //flush_clflushopt(static_cast<char*>(entry.data), 1);

                    break;
                case 4:
                    _mm_stream_si32((int*) entry.dax_addr, (int) entry.data);

                    #ifdef FORCE_FLUSHING
                    _mm_clflushopt(ALIGN_TO_64(static_cast<const char*>(dev_addr)));
                    _mm_sfence();
                    #endif
                    break;
                case 8:
                    _mm_stream_pi((__m64*) entry.dax_addr, (__m64) entry.data);

                    #ifdef FORCE_FLUSHING
                    _mm_clflushopt(ALIGN_TO_64(static_cast<const char*>(dev_addr)));
                    _mm_sfence();
                    #endif

                    //flush_clflushopt(static_cast<char*>(entry.data), 8);

                    //flush_clflushopt(dev_addr, 8);

                    break;

                default:
                    std::cerr << "Unsupported op size " << entry.op_size << "!" << std::endl;

                    pthread_exit(NULL);
                    break;
                }

                //total_bytes += entry.op_size;
            } else if (entry.op == TraceOperation::CLFLUSH) {
                _mm_clflushopt(static_cast<void*>(dev_addr));
                _mm_sfence();
                //total_bytes += 8;
            } else {
                assert(false);
            }
            (void) temp_var;
        }
    }

    const auto time_stop = std::chrono::high_resolution_clock::now();
    
    struct io_stat *stat = &(args->stat);
    stat->latency_sum += std::chrono::duration_cast<std::chrono::nanoseconds>(time_stop - time_start).count();
    
    stat->read_bytes += (args->trace_file->get_total(TraceOperation::READ) * i);
    stat->write_bytes += (args->trace_file->get_total(TraceOperation::WRITE) * i);
    stat->total_bytes += (stat->read_bytes + stat->write_bytes);


    pthread_exit(NULL);
}

void BenchSuite::run(const size_t replay_rounds)
{
    

    std::cout << "DAX area: [" << std::hex << this->mem_area << '-' << (void*) ((uintptr_t) this->mem_area + this->mem_size) << ']' << std::endl;

    // Calculate the DAX addresses based on the offset inside the trace PMEM region.
    for (TraceEntry &entry : this->trace_file) {
        entry.dax_addr = static_cast<char*>(this->mem_area) + entry.rel_addr;
    }


    // Spawn the threads
    pthread_t threads[this->num_threads] = {};
    struct WorkerArguments thread_args[this->num_threads] = {{(&(this->trace_file)), replay_rounds}};

    std::cout << "Initializing " << this->num_threads << " threads ..." << std::endl;

    this->drop_caches();

    int rc;
    for (size_t i = 0; i < this->num_threads; ++i) {
        rc = pthread_create(&threads[i], NULL, do_work, static_cast<void*>(&(thread_args[i])));

        if (rc) {
            std::cerr << "Unable to create thread" << std::endl;
            deallocate_mem_area();
            return;
        }
    }

    for (size_t i = 0; i < this->num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

    for (size_t i = 0; i < this->num_threads; ++i) {
        std::cout << "Thread " << i << " -> Latency: " << std::dec << thread_args[i].stat.latency_sum << " us (" << (static_cast<double>(thread_args[i].stat.latency_sum) / 1'000'000'000) << " sec)" <<
            " Read: " << (static_cast<double>(thread_args[i].stat.read_bytes) / Mebibyte) << " MB" <<
            " Write: " << (static_cast<double>(thread_args[i].stat.write_bytes) / Mebibyte) << " MB" <<
            " Total bytes: "  << (static_cast<double>(thread_args[i].stat.total_bytes) / Mebibyte) << " MB"
            " Bandwidth: " << (static_cast<double>(thread_args[i].stat.total_bytes) / Gibibyte / (thread_args[i].stat.latency_sum / 1'000'000'000)) << " GB/s" << std::endl;
    }

    

    // unsigned int z = 0;

    // for (const auto &trace_entry : trace_file) {
    //     std::cout << ((trace_entry.op == TraceOperation::READ) ? "R" : "W") << " 0x" << std::hex << trace_entry.addr << " ";

    //     for (size_t i = 0; i < trace_entry.data.size(); ++i) {
    //         std::cout << std::hex << static_cast<unsigned int>(trace_entry.data[i]);
    //     }

    //     std::cout << std::endl;

    //     if ((z++) >= 100)
    //         break;
    // }
}

