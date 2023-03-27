#include "bench_suite.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <cassert>

#include <immintrin.h>
#include <xmmintrin.h>


static constexpr size_t CACHE_LINE_SIZE = 64;

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
    int fd = open("/dev/dax0.0", O_RDWR);

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

    if (mlock(mem_area, this->mem_size) < 0) {
        std::cerr << "Warning: unable to mlock DRAM-backed memory region" << std::endl;
    }

    this->mem_area = mem_area;

    return true;
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


static void* do_work(void *arg)
{
    //TraceFile* trace_file = static_cast<TraceFile*>(arg);
    struct WorkerArguments *args = static_cast<struct WorkerArguments*>(arg);
    char* write_addr = nullptr;

    std::cout << "here" << std::endl;

    std::cout << "num rounds: " << args->replay_rounds << std::endl;


    for (size_t i = 0; args->replay_rounds + 1; ++i) {

        for (const TraceEntry& entry : *(args->trace_file)) {
            write_addr = static_cast<char*>(entry.dax_addr);
            //std::cout << "write_addr: " << (void*) write_addr << std::endl;

    
            std::cout << entry.op_size << " " << entry.data.size() << std::endl;
            std::cout << (unsigned long) write_addr << std::endl;
            assert(entry.op_size <= entry.data.size());

            switch (entry.op_size)
            {
            case 1:
                *(write_addr) = entry.data[0];
                flush_clflushopt(write_addr, 1);
                _mm_sfence();

                break;
            case 4:
                *(write_addr) = (((uint32_t)entry.data[0] << 24) | ((uint32_t)entry.data[1] << 16) | ((uint32_t)entry.data[2] << 8) | (uint32_t) entry.data[3]);
                flush_clflushopt(write_addr, 4);
                _mm_sfence();

                break;
            case 8:
                 *(write_addr) = ((uint64_t)entry.data[0] << 56) | ((uint64_t)entry.data[1] << 48) | ((uint64_t)entry.data[2] << 40) | ((uint64_t)entry.data[3] << 32)
                                     | ((uint64_t)entry.data[4] << 24) | ((uint64_t)entry.data[5] << 16) | ((uint64_t)entry.data[6] << 8) | (uint64_t)entry.data[7];

                flush_clflushopt(write_addr, 8);
                _mm_sfence();

                break;

            default:
                std::cerr << "Unsupported op size " << entry.op_size << "!" << std::endl;

                pthread_exit(NULL);
                break;
            }
        }
    }


    pthread_exit(NULL);
}

void BenchSuite::run(const size_t replay_rounds)
{
    if (!allocate_pmem_area()) {
        std::cerr << "Unable to allocate DAX-backed region, switching to main memory backed area..." << std::endl;

        if (!allocate_dram_area()) {
            std::cerr << "Unable to allocate DRAM-backed region, exiting..." << std::endl;
            return;
        }
    }

    std::cout << "DAX area: [" << std::hex << this->mem_area << '-' << (void*) ((uintptr_t) this->mem_area + this->mem_size) << ']' << std::endl;

    // Calculate the DAX addresses based on the offset inside the trace PMEM region.
    for (TraceEntry &entry : this->trace_file) {
        entry.dax_addr = static_cast<char*>(this->mem_area) + entry.rel_addr;
    }


    // Spawn the threads
    pthread_t threads[this->num_threads] = {};
    struct WorkerArguments thread_args[this->num_threads] = {{(&(this->trace_file)), replay_rounds}};

    std::cout << "Initializing " << this->num_threads << " threads ..." << std::endl;

    
    int rc;
    for (size_t i = 0; i < this->num_threads; ++i) {
        std::cout << thread_args[i].trace_file->get_total(TraceOperation::WRITE) << std::endl;
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

    // drop_caches();

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

    deallocate_mem_area();
}