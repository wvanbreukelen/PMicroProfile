#include "bench_suite.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <cassert>
#include <chrono>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>

#include <immintrin.h>

#include "worker.hpp"
#include "bench_export.hpp"
#include "pmc.hpp"


#define likely(x)    __builtin_expect (!!(x), 1)
#define unlikely(x)  __builtin_expect (!!(x), 0)

#define Mebibyte (1024 * 1024)
#define Gibibyte (1024 * 1024 * 1024)

#define ALIGN_TO_64(ptr) ((char*)(((uintptr_t)(ptr) + 63) & ~(uintptr_t)63))
//#define STRICT_CONSISTENCY

static constexpr size_t CACHE_LINE_SIZE = 64;

#define SAMPLE_RATE   1000000
#define SAMPLE_LENGTH 10000
#define ENABLE_DCOLLECTION

#define EVENT_UNC_M_PMM_WPQ_INSERTS 0x7E
#define EVENT_UNC_M_PMM_RPQ_INSERTS 0xE3
#define EVENT_UNC_M_PMM_RPQ_OCCUPANCY_ALL 0x1E0 // umask=0x1,event=0xE0


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


static long perf_event_open(struct perf_event_attr* event_attr, pid_t pid, int cpu,
		     int group_fd, unsigned long flags)
{
    return syscall(__NR_perf_event_open, event_attr, pid, cpu, group_fd, flags);
}


static int attach_imc_probe(const unsigned int imc_id, const unsigned int event_id)
{
    struct perf_event_attr pe;

    long long count;
    int fd;

    memset(&pe, 0, sizeof(struct perf_event_attr));

    pe.type = imc_id;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = event_id;
    pe.sample_type = PERF_SAMPLE_IDENTIFIER;
    //pe.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;
    pe.disabled = 1;
    pe.inherit = 1;
    pe.exclude_guest = 0;
    pe.exclude_host = 0;

    fd = perf_event_open(&pe, -1, 0, -1, 0);

    if (fd == -1) {
        std::cerr << "[iMC " << imc_id << "] Unable to open perf event monitor for event config: 0x" << std::hex << pe.config << " errno: " << std::dec << errno << std::endl;
        //exit(EXIT_FAILURE);
    } else {
	std::cout << "fd: " << fd << std::endl;
    }

    return fd;
}


// static double measure_ewr()
// {
//     return 0.0;
// }//pe.sample_period = 1000; // FIXME: which unit? https://stackoverflow.com/questions/45299059/how-can-i-sample-at-constant-rate-with-perf-event-open
//  

// static double measure_ebr()
// {
//     return 0.0;
// }

// static double measure_hit_ratio_xpbuffer()
// {
//     return 0.0;
// }


static __inline__ unsigned long long rdtsc(void)
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}


static void* do_work(void *arg)
{
    struct WorkerArguments *args = static_cast<struct WorkerArguments*>(arg);
    struct io_stat *stat = &(args->stat);

   
    //probe_reset(wpq_probe);

    size_t count = 0;
    volatile unsigned long long temp_var = 0;
    
    const auto time_start = std::chrono::high_resolution_clock::now();
    size_t total_bytes = 0;
    // TraceOperation prev_op = TraceOperation::UNKNOWN;
    // char *prev_addr = nullptr;
    // size_t stride_write_size = 0;

    struct io_sample *cur_sample;
    size_t sample_pos = 0;
    unsigned long long total_wpq_count = 0;



    cur_sample = &(stat->samples[0]);

    size_t i = 0;
    unsigned long long latest_sample_time = __builtin_ia32_rdtsc();
    for (; i < args->replay_rounds + 1; ++i) {
        //prev_op = (*args->trace_file)[0].op;

        //void* dev_addr = nullptr;
        //uint64_t data;
        bool is_sampling = false;
        unsigned long long cur_time;
        size_t z = 0;

        //size_t num_reads = 0, num_writes = 0, num_flushes = 0;
        //unsigned long long read_sum_cycles = 0, write_sum_cycles = 0, flush_sum_cycles = 0;

        //probe_enable(wpq_probe);

        for (const TraceEntry& entry : *(args->trace_file)) {
            #ifdef ENABLE_DCOLLECTION
            // Calling rdtsc each iteration is quite expensive; it takes +/- 30 cycles, therefore, we only
            // check the rdtsc counter every 8 iterations (i.e. (z % 8) == 0))
            if (((z & 0x08) == 0)) {
                cur_time = __builtin_ia32_rdtsc();
                if (is_sampling && (cur_time - latest_sample_time) >= SAMPLE_LENGTH) {
                    //probe_disable(wpq_probe);
                    //probe_count(wpq_probe, &total_wpq_count);

                    is_sampling = false;

                    cur_sample->time_since_start = std::chrono::duration_cast<std::chrono::nanoseconds>((std::chrono::high_resolution_clock::now() - time_start));
                    cur_sample++;
                    ++(stat->num_collected_samples);
                    assert(stat->num_collected_samples < MAX_SAMPLES);
                    
                    latest_sample_time = cur_time;
                } else if ((cur_time - latest_sample_time) >= SAMPLE_RATE) {
                    is_sampling = true;
                    latest_sample_time = cur_time;

                    //probe_enable(wpq_probe);
                }
            }
            ++z;
            #endif

            //std::cout << std::dec << (__builtin_ia32_rdtsc() - latest_sample_time) << std::endl;
            //dev_addr = entry.dax_addr;
            //data = entry.data;

            switch (entry.op) {
                case TraceOperation::READ:
                {
                    switch (entry.op_size)
                    {
                        case 1:
                        {
                            #ifdef ENABLE_DCOLLECTION
                            if (is_sampling) {
                                const unsigned long long start_ticks = __builtin_ia32_rdtsc();
                                *(reinterpret_cast<volatile char*>(&temp_var)) = *(static_cast<char*>(entry.dax_addr));
                                cur_sample->read_inst_cycles += (__builtin_ia32_rdtsc() - start_ticks);
                            } else {
                                *(reinterpret_cast<volatile char*>(&temp_var)) = *(static_cast<char*>(entry.dax_addr));
                            }
                            #else
                                *(reinterpret_cast<volatile char*>(&temp_var)) = *(static_cast<char*>(entry.dax_addr));
                            #endif

                            break;
                        }
                        case 4:
                        {
                            #ifdef ENABLE_DCOLLECTION
                            if (is_sampling) {
                                const unsigned long long start_ticks = __builtin_ia32_rdtsc();
                                *(reinterpret_cast<volatile int*>(&temp_var)) = *(static_cast<char*>(entry.dax_addr));
                                cur_sample->read_inst_cycles += (__builtin_ia32_rdtsc() - start_ticks);
                            } else {
                                *(reinterpret_cast<volatile int*>(&temp_var)) = *(static_cast<char*>(entry.dax_addr));
                            }
                            #else
                                *(reinterpret_cast<volatile int*>(&temp_var)) = *(static_cast<char*>(entry.dax_addr));
                            #endif

                            break;
                        }
                        case 8:
                        {
                            #ifdef ENABLE_DCOLLECTION
                            if (is_sampling) {
                                const unsigned long long start_ticks = __builtin_ia32_rdtsc();
                                *(reinterpret_cast<volatile long*>(&temp_var)) = *(static_cast<char*>(entry.dax_addr));
                                cur_sample->read_inst_cycles += (__builtin_ia32_rdtsc() - start_ticks);
                            } else {
                                *(reinterpret_cast<volatile long*>(&temp_var)) = *(static_cast<char*>(entry.dax_addr));
                            }
                            #else
                                *(reinterpret_cast<volatile long*>(&temp_var)) = *(static_cast<char*>(entry.dax_addr));
                            #endif

                            break;
                        }
                        default:
                            std::cerr << "Unsupported op size " << entry.op_size << "!" << std::endl;

                            pthread_exit(NULL);
                            break;
                    }

                    #ifdef ENABLE_DCOLLECTION
                    if (is_sampling) {
                        ++(cur_sample->num_reads);
                        cur_sample->bytes_read += entry.op_size;
                    }
                    #endif

                    break;
                }

                case TraceOperation::WRITE:
                {
                    switch (entry.op_size)
                    {
                    case 1:
                    {
                        #ifdef ENABLE_DCOLLECTION
                        if (is_sampling) {
                            const unsigned long long start_ticks = __builtin_ia32_rdtsc();
                            #ifdef STRICT_CONSISTENCY
                            _mm_sfence();
                            #endif

                            *(static_cast<unsigned char*>(entry.dax_addr)) = static_cast<unsigned char>(entry.data);

                            #ifdef STRICT_CONSISTENCY
                            _mm_clflushopt(static_cast<void*>(dev_addr));
                            _mm_sfence();
                            #endif

                            cur_sample->write_inst_cycles += (__builtin_ia32_rdtsc() - start_ticks);
                        } else {
                            #ifdef STRICT_CONSISTENCY
                            _mm_sfence();
                            #endif

                            *(static_cast<unsigned char*>(entry.dax_addr)) = static_cast<unsigned char>(entry.data);

                            #ifdef STRICT_CONSISTENCY
                            _mm_clflushopt(static_cast<void*>(dev_addr));
                            _mm_sfence();
                            #endif
                        }
                        #else
                            *(static_cast<unsigned char*>(entry.dax_addr)) = static_cast<unsigned char>(entry.data);
                        #endif

                        break;
                    }
                    case 4:
                    {
                        #ifdef ENABLE_DCOLLECTION
                        if (is_sampling) {
                            const unsigned long long start_ticks = __builtin_ia32_rdtsc();
                            #ifdef STRICT_CONSISTENCY
                            _mm_sfence();
                            #endif

                            //_mm_stream_pi((__m64*) entry.dax_addr, (__m64) entry.data);
                            _mm_stream_si32(static_cast<int*>(entry.dax_addr), static_cast<int>(entry.data));

                            #ifdef STRICT_CONSISTENCY
                            _mm_clflushopt(static_cast<void*>(entry.dax_addr));
                            _mm_sfence();
                            #endif

                            cur_sample->write_inst_cycles += (__builtin_ia32_rdtsc() - start_ticks);
                        } else {
                            #ifdef STRICT_CONSISTENCY
                            _mm_sfence();
                            #endif

                            //_mm_stream_pi((__m64*) entry.dax_addr, (__m64) entry.data);
                            _mm_stream_si32(static_cast<int*>(entry.dax_addr), static_cast<int>(entry.data));

                            #ifdef STRICT_CONSISTENCY
                            _mm_clflushopt(static_cast<void*>(entry.dax_addr));
                            _mm_sfence();
                            #endif
                        }
                        #else
                            #ifdef STRICT_CONSISTENCY
                                _mm_sfence();
                                #endif

                                //_mm_stream_pi((__m64*) entry.dax_addr, (__m64) entry.data);
                                _mm_stream_si32(static_cast<int*>(entry.dax_addr), static_cast<int>(entry.data));

                                #ifdef STRICT_CONSISTENCY
                                _mm_clflushopt(static_cast<void*>(dev_addr));
                                _mm_sfence();
                                #endif
                        #endif

                        break;
                    }
                    case 8:
                    {
                        #ifdef ENABLE_DCOLLECTION
                        if (is_sampling) {
                            const unsigned long long start_ticks = __builtin_ia32_rdtsc();
                            #ifdef STRICT_CONSISTENCY
                            _mm_sfence();
                            #endif

                            _mm_stream_pi(static_cast<__m64*>(entry.dax_addr), reinterpret_cast<__m64>(entry.data));
                            

                            #ifdef STRICT_CONSISTENCY
                            _mm_clflushopt(static_cast<void*>(dev_addr));
                            _mm_sfence();
                            #endif

                            cur_sample->write_inst_cycles += (__builtin_ia32_rdtsc() - start_ticks);
                        } else {
                            #ifdef STRICT_CONSISTENCY
                            _mm_sfence();
                            #endif

                            _mm_stream_pi(static_cast<__m64*>(entry.dax_addr), reinterpret_cast<__m64>(entry.data));
                            

                            #ifdef STRICT_CONSISTENCY
                            _mm_clflushopt(static_cast<void*>(dev_addr));
                            _mm_sfence();
                            #endif
                        }
                        #else
                             #ifdef STRICT_CONSISTENCY
                            _mm_sfence();
                            #endif

                            _mm_stream_pi(static_cast<__m64*>(entry.dax_addr), reinterpret_cast<__m64>(entry.data));
                            
                            #ifdef STRICT_CONSISTENCY
                            _mm_clflushopt(static_cast<void*>(dev_addr));
                            _mm_sfence();
                            #endif
                        #endif

                        break;
                    }
                    default:
                        std::cerr << "Unsupported op size " << entry.op_size << "!" << std::endl;

                        pthread_exit(NULL);
                        break;
                    }

                    #ifdef ENABLE_DCOLLECTION
                    if (is_sampling) {
                        ++(cur_sample->num_writes);
                        cur_sample->bytes_written += entry.op_size;
                    }
                    #endif

                    break;
                }

                case TraceOperation::CLFLUSH:
                {
                    #ifdef ENABLE_DCOLLECTION
                        if (is_sampling) {
                            unsigned long long start_ticks = __builtin_ia32_rdtsc();
                            _mm_sfence();
                            _mm_clflushopt(entry.dax_addr);
                            _mm_sfence();
                            cur_sample->flush_inst_cycles += (__builtin_ia32_rdtsc() - start_ticks);
                            ++(cur_sample->num_flushes);
                        } else {
                            _mm_sfence();
                            _mm_clflushopt(entry.dax_addr);
                            _mm_sfence();
                        }
                    #else
                    _mm_sfence();
                    _mm_clflushopt(entry.dax_addr);
                    _mm_sfence();
                    #endif

                    assert(false);

                    break;
                }

                default:
                    std::cerr << "Unknown operation" << std::endl;
                    assert(false);
                    break;
            }

            total_bytes += entry.op_size;
        }
    }
    
    // Do a simple addition to make sure the compiler does not optimize 'temp_var' away.
    temp_var++;
    const auto time_stop = std::chrono::high_resolution_clock::now();

   

    stat->latency_sum += std::chrono::duration_cast<std::chrono::nanoseconds>(time_stop - time_start).count();
    
    stat->read_bytes += (args->trace_file->get_total(TraceOperation::READ) * i);
    stat->write_bytes += (args->trace_file->get_total(TraceOperation::WRITE) * i);
    stat->total_bytes += (stat->read_bytes + stat->write_bytes);

    //std::cout << "WPQ count: " << std::dec << total_wpq_count << std::endl;


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

    assert(sysconf(_SC_NPROCESSORS_ONLN) > static_cast<long>(this->num_threads));

    

    // for (size_t i = 0; i < num_imcs; ++i) {
    //     int fd = attach_imc_probe(imc_ids[i], EVENT_UNC_M_PMM_WPQ_INSERTS);
    //     close(fd);
    // }

    // for (size_t i = 0; i < num_imcs; ++i) {
    //     int fd = attach_imc_probe(imc_ids[i], EVENT_UNC_M_PMM_RPQ_INSERTS);
    //     close(fd);
    // }

    // for (size_t i = 0; i < num_imcs; ++i) {
    //     int fd = attach_imc_probe(imc_ids[i], EVENT_UNC_M_PMM_RPQ_OCCUPANCY_ALL);
    //     close(fd);
    // }

    // TODO: for each of the iMC's, capture unc_m_pmm_wpq_inserts and unc_m_pmm_rpq_inserts

    //int fd = setup_perf_events(getpid());

    //return;

    // PMC pmc;

    // if (!pmc.init()) {
    //     std::cerr << "Failed to initialize PMC!" << std::endl;
    //     pthread_exit(NULL);
    // }

    // struct iMCProbe wpq_probe{};
    // if (!pmc.add_imc_probe(EVENT_UNC_M_PMM_WPQ_INSERTS, wpq_probe)) {
    //     std::cerr << "Unable to add probe!" << std::endl;
    //     pthread_exit(NULL);
    // }

    // probe_reset(wpq_probe);
    // probe_enable(wpq_probe);

    struct perf_event_attr pe;
    long long count;
    int fd;

    memset(&pe, 0, sizeof(pe));
    pe.type = 13;
    pe.size = sizeof(pe);
    pe.config = 0x7E;
    pe.sample_type = PERF_SAMPLE_IDENTIFIER;
    pe.disabled = 1;
    pe.inherit = 1;
    pe.exclude_kernel = 0;
    pe.exclude_host = 0;
    pe.exclude_hv = 1;

    fd = perf_event_open(&pe, 0, -1, -1, 0);
    if (fd == -1) {
        fprintf(stderr, "Error opening leader %llx\n", pe.config);
        exit(EXIT_FAILURE);
    }

    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);


    pthread_attr_t attr;
    cpu_set_t cpus;
    pthread_attr_init(&attr);

    int rc;
    for (size_t i = 0; i < this->num_threads; ++i) {
        thread_args[i].init();

        // Pin thread on core
        CPU_ZERO(&cpus);
        CPU_SET(i, &cpus);

        if (pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus)) {
            std::cerr << "Unable to set core affinity to core " << i << std::endl;
        }
        

        // https://www.strchr.com/performance_measurements_with_rdtsc
        // !!! We might need to fix CPU frequency, maybe we can set the CPU governor to performance and disable turbo?
        // Disabling hyperthreading will be difficult though.
        rc = pthread_create(&threads[i], &attr, do_work, static_cast<void*>(&(thread_args[i])));

        if (rc) {
            std::cerr << "Unable to create thread" << std::endl;
            deallocate_mem_area();
            return;
        }
    }

    for (size_t i = 0; i < this->num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

    ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    read(fd, &count, sizeof(count));

    std::cout << "Count: " << std::dec << count << std::endl;

    // probe_disable(wpq_probe);
    // probe_count(wpq_probe, &total_count);

    // if (!pmc.remove_imc_probe(wpq_probe)) {
    //     std::cerr << "Unable to remove iMC probes!" << std::endl;
    // }

    const struct io_stat* thread_stat = nullptr;

    for (size_t i = 0; i < this->num_threads; ++i) {
        thread_stat = &(thread_args[i].stat);

        std::cout << "Thread " << i << " -> Latency: " << std::dec << thread_stat->latency_sum << " us (" << (static_cast<double>(thread_stat->latency_sum) / 1'000'000'000) << " sec)" <<
            " Read: " << (static_cast<double>(thread_stat->read_bytes) / Mebibyte) << " MB" <<
            " Write: " << (static_cast<double>(thread_stat->write_bytes) / Mebibyte) << " MB" <<
            " Total bytes: "  << (static_cast<double>(thread_stat->total_bytes) / Mebibyte) << " MB"
            " Bandwidth: " << (static_cast<double>(thread_stat->total_bytes) / Gibibyte / (thread_stat->latency_sum / 1'000'000'000)) << " GB/s" << std::endl;

        // if (thread_stat->num_reads > 0)
        //     std::cout << "Avg. read latency: " << (thread_stat->read_inst_cycles / thread_stat->num_reads) << " cycles" <<  std::endl;
        // if (thread_stat->num_writes > 0)
        //     std::cout << "Avg. write latency: " << (thread_stat->write_inst_cycles / thread_stat->num_writes) << " cycles" << std::endl;
        // if (thread_stat->num_flushes > 0)
        //     std::cout << "Avg. flush latency: " << (thread_stat->flush_inst_cycles / thread_stat->num_flushes) << " cycles" << std::endl;

        std::cout << "Collected " << thread_stat->num_collected_samples << " samples!" << std::endl;
    }

    
    for (size_t i = 0; i < this->num_threads; ++i) {
        BenchExport bench_export(thread_args[i]);

        bench_export.export_io_stat("test.csv");
    }
    
}

