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



#include "worker.hpp"
#include "bench_export.hpp"
#include "pmc.hpp"
#include "io.hpp"

std::chrono::time_point<std::chrono::high_resolution_clock> time_start;

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

    //size_t page_size = sysconf(_SC_PAGESIZE);

    if (madvise(dax_area, this->mem_size, MADV_WILLNEED) < 0)
	    std::cerr << "Warning: unable to perform madvice on DAX region." << std::endl;
    //for (char* dax_ptr = static_cast<char*>(dax_area); dax_ptr < static_cast<char*>(dax_area) + this->mem_size; dax_ptr += page_size)
	//madvise(dax_ptr, page_size, MADV_WILLNEED);

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

    if (madvise(mem_area, this->mem_size, MADV_WILLNEED) < 0)
        std::cerr << "Warning: unable to MADV_WILLNEED DRAM-backed memory region" << std::endl;


    this->mem_area = mem_area;

    return true;
}

void BenchSuite::allocate_mem_area()
{
    if (force_ram) {
        if (!allocate_dram_area()) {
            std::cerr << "Unable to allocate DRAM-backed region, exiting..." << std::endl;
            exit(1);
            return;
        }

        return;
    }

    if (!allocate_pmem_area()) {
        std::cerr << "Unable to allocate DAX-backed region at " << this->pmem_device_loc << "!" << std::endl;
        std::cerr << "Please make sure this system has set up PMEM as a DAX-enabled device using `ndctl list`." << std::endl;

        if (!this->do_fallback_ram) {
            std::cerr << "Exiting. Fallback to RAM can be enabled by setting the --fallback-ram flag." << std::endl;
            exit(1);
            return;
        }

        if (!allocate_dram_area()) {
            std::cerr << "Unable to allocate DRAM-backed region, exiting..." << std::endl;
            exit(1);
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
   // pe.sample_type = PERF_SAMPLE_IDENTIFIER;
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

static inline uint64_t next_pow2_fast(uint64_t x)
{

    #if defined(__GNUC__) || defined(__GNUG___)
    // ref: https://jameshfisher.com/2018/03/30/round-up-power-2/
    return (x == 1) ? 1 : 1 << (64 - __builtin_clzl(x - 1));
    #else
    x |= x>>1;
    x |= x>>2;
    x |= x>>4;
    x |= x>>8;
    x |= x>>16;
    x |= x>>32;

    return x;
    #endif
}


static void replay_trace(TraceFile &trace_file, PMC &pmc, struct io_sample** cur_sample, ssize_t* total_bytes, unsigned long long *_latest_sample_time, struct io_stat* stat)
{
    constexpr uint64_t sample_mask = (1024 - 1);
    bool is_sampling = false;
    unsigned long long cur_time;
    unsigned long long latest_sample_time = *(_latest_sample_time);
    size_t z = 0;



    for (const TraceEntry& entry : trace_file) {
        #ifdef ENABLE_DCOLLECTION
        if (unlikely((z & sample_mask) == 0)) {  // (cur_time - latest_sample_time) >= SAMPLE_LENGTH
            cur_time = __builtin_ia32_rdtsc();

            if (is_sampling) {
                if ((cur_time - latest_sample_time) >= SAMPLE_PERIOD_ON) {
                    pmc.disable_probes();

                    is_sampling = false;
                    (*cur_sample)->time_since_start = std::chrono::duration_cast<std::chrono::nanoseconds>((std::chrono::high_resolution_clock::now() - time_start));

                    pmc.get_probe(EVENT_UNC_M_CLOCKTICKS).probe_count_single(&((*cur_sample)->unc_ticks));
                    pmc.get_probe(EVENT_UNC_M_PMM_RPQ_INSERTS).probe_count(&((*cur_sample)->rpq_inserts));
                    pmc.get_probe(EVENT_UNC_M_PMM_WPQ_INSERTS).probe_count(&((*cur_sample)->wpq_inserts));
                    pmc.get_probe(EVENT_UNC_M_PMM_WPQ_OCCUPANCY_ALL).probe_count(&((*cur_sample)->wpq_occupancy));
                    pmc.get_probe(EVENT_UNC_M_PMM_RPQ_OCCUPANCY_ALL).probe_count(&((*cur_sample)->rpq_occupancy));

                    pmc.get_probe(EVENT_MEM_LOAD_L3_MISS_RETIRED_LOCAL_PMM).probe_count_single(&((*cur_sample)->l3_misses_local_pmm));
                    pmc.get_probe(EVENT_MEM_LOAD_L3_MISS_RETIRED_REMOTE_PMM).probe_count_single(&((*cur_sample)->l3_misses_remote_pmm));

                    (*cur_sample)->total_bytes_read_write = *(total_bytes);

                    (*cur_sample)++;
                    ++(stat->num_collected_samples);

                    if (stat->num_collected_samples > MAX_SAMPLES) {
                        std::cerr << "Number of collected sample exteeds MAX_SAMPLES (= " << std::dec << MAX_SAMPLES << "), please increase!" << std::endl;
                        pthread_exit(NULL);
                    }
                    
                    latest_sample_time = cur_time;
                }
            } else {
                if ((cur_time - latest_sample_time) >= SAMPLE_PERIOD_OFF) {
                    is_sampling = true;
                    latest_sample_time = cur_time;

                    pmc.reset_probes();
                    pmc.enable_probes();
                }
            }
        }

        ++z;
        #endif

        switch (entry.op) {
            case TraceOperation::READ:
            {
                switch (entry.op_size)
                {
                    case 1:
                    {
                        read_value<uint8_t>(entry, is_sampling, (*cur_sample));
                        break;
                    }
                    case 4:
                    {
                        read_value<uint32_t>(entry, is_sampling, (*cur_sample));
                        break;
                    }
                    case 8:
                    {
                        read_value<uint64_t>(entry, is_sampling, (*cur_sample));
                        break;
                    }
                    case 16:
                    {
                        read_value<__uint128_t>(entry, is_sampling, (*cur_sample));
                        break;
                    }
                    default:
                        std::cerr << "Unsupported op size " << std::dec << entry.op_size << "!" << std::endl;
                        
                        pthread_exit(NULL);
                        break;
                }

                #ifdef ENABLE_DCOLLECTION
                if (is_sampling) {
                    ++((*cur_sample)->num_reads);
                    (*cur_sample)->bytes_read += entry.op_size;
                }
                #endif

                break;
            }

            case TraceOperation::WRITE:
            {
                switch (entry.opcode)
                {
                case 0xA4: // 1 byte size
                {
                    write_mov_8(entry, is_sampling, (*cur_sample));
                    break;
                }
                case 0x4444: // 4 bytes - MOVNTI
                {
                    write_movnti_32(entry, is_sampling, (*cur_sample));
                    break;
                }
                case 0xC30F: // 8 bytes - MOVNTQ
                {
                    write_movntq_64(entry, is_sampling, (*cur_sample));
                    break;
                }
                case 0xE70F: // 16 bytes - MOVNTDQ
                {
                    write_movntqd_128(entry, is_sampling, (*cur_sample));
                    break;
                }
                case 0x2B0F: // 16 bytes - MOVNTPS/MOVNTPD
                {
                    write_movntps_128(entry, is_sampling, (*cur_sample));
                    break;
                }

                default:
                    std::cerr << "Unsupported operation 0x" << std::hex << entry.opcode << "!" << std::endl;
                    std::cout << entry << std::endl;

                    pthread_exit(NULL);
                    break;
                }

                #ifdef ENABLE_DCOLLECTION
                if (is_sampling) {
                    ++((*cur_sample)->num_writes);
                    (*cur_sample)->bytes_written += entry.op_size;
                }
                #endif

                break;
            }
            case TraceOperation::CLFLUSH:
            {
                flush_clflush(entry, is_sampling, *cur_sample);
                break;
            }
            case TraceOperation::MFENCE:
            {
                _mm_mfence();
                break;
            }
            case TraceOperation::SFENCE:
            {
                _mm_sfence();
                break;
            }
            case TraceOperation::LFENCE:
            {
                _mm_lfence();
                break;
            }

            default:
                std::cerr << "Error: Unknown operation" << std::endl;
                assert(false);
                break;
        }

        *total_bytes += entry.op_size;
    }

    *(_latest_sample_time) = latest_sample_time;
}

static void* do_work(void *arg)
{
    struct WorkerArguments *args = static_cast<struct WorkerArguments*>(arg);
    struct io_stat *stat = &(args->stat);

    size_t count = 0;
    ssize_t total_bytes = 0;

    struct io_sample *cur_sample;
    size_t sample_pos = 0;
    //unsigned long long total_wpq_count = 0;


    PMC pmc;

    if (!pmc.init()) {
        std::cerr << "Failed to initialize PMC!" << std::endl;
        pthread_exit(NULL);
    }

    #ifdef ENABLE_DCOLLECTION
    //struct iMCProbe unc_ticks_probe{}, wpq_probe{}, rpq_probe{}, wpq_occupancy_probe{}, rpq_occupancy_probe{};

    if (!pmc.add_imc_probe(EVENT_UNC_M_CLOCKTICKS)) {
        std::cerr << "Unable to add EVENT_UNC_M_CLOCKTICKS probe!" << std::endl;
        //pthread_exit(NULL);
    }

    if (!pmc.add_imc_probe(EVENT_UNC_M_PMM_WPQ_INSERTS)) {
        std::cerr << "Unable to add EVENT_UNC_M_PMM_WPQ_INSERTS probe!" << std::endl;
        //pthread_exit(NULL);
    }

    if (!pmc.add_imc_probe(EVENT_UNC_M_PMM_RPQ_INSERTS)) {
        std::cerr << "Unable to add EVENT_UNC_M_PMM_RPQ_INSERTS probe!" << std::endl;
        //pthread_exit(NULL);
    }

    if (!pmc.add_imc_probe(EVENT_UNC_M_PMM_WPQ_OCCUPANCY_ALL)) {
        std::cerr << "Unable to add EVENT_UNC_M_PMM_WPQ_OCCUPANCY_ALL probe!" << std::endl;
        //pthread_exit(NULL);
    }

    if (!pmc.add_imc_probe(EVENT_UNC_M_PMM_RPQ_OCCUPANCY_ALL)) {
        std::cerr << "Unable to add EVENT_UNC_M_PMM_RPQ_OCCUPANCY_ALL probe!" << std::endl;
        //pthread_exit(NULL);
    }

    if (!pmc.add_oncore_probe(EVENT_MEM_LOAD_L3_MISS_RETIRED_LOCAL_PMM, pthread_self())) {
        std::cerr << "Unable to add EVENT_MEM_LOAD_L3_MISS_RETIRED_LOCAL_PMM probe!" << std::endl;
        //pthread_exit(NULL);
    }

    if (!pmc.add_oncore_probe(EVENT_MEM_LOAD_L3_MISS_RETIRED_REMOTE_PMM, pthread_self())) {
        std::cerr << "Unable to add EVENT_MEM_LOAD_L3_MISS_RETIRED_REMOTE_PMM probe!" << std::endl;
        //pthread_exit(NULL);
    }


    // unc_ticks_probe.probe_reset();
    // wpq_probe.probe_reset();
    // rpq_probe.probe_reset();
    // wpq_occupancy_probe.probe_reset();
    // rpq_occupancy_probe.probe_reset();
    pmc.reset_probes();
    #endif
    //probe_enable(wpq_probe);

    cur_sample = &(stat->samples[0]);

    size_t i = 0;
    //const uint64_t sample_mask = next_pow2_fast(args->trace_file->size() / args->num_samples) - 1;
    
    unsigned long long latest_sample_time = 0;
    io_stat dummy_stat;

    for (size_t round = 0; round < args->cache_warming_rounds; ++round)
        replay_trace(*args->trace_file, pmc, &cur_sample, &total_bytes, &latest_sample_time, &dummy_stat);

    time_start = std::chrono::high_resolution_clock::now();
    latest_sample_time = __builtin_ia32_rdtsc();
    total_bytes = 0;
    sample_pos = 0;
    cur_sample = &(stat->samples[0]);

    for (; i < args->replay_rounds + 1; ++i)
        replay_trace(*args->trace_file, pmc, &cur_sample, &total_bytes, &latest_sample_time, stat);
    
    const auto time_stop = std::chrono::high_resolution_clock::now();

    #ifdef ENABLE_DCOLLECTION
    pmc.disable_probes();
    pmc.remove_imc_probes();
    #endif

    stat->latency_sum += std::chrono::duration_cast<std::chrono::nanoseconds>(time_stop - time_start).count();
    
    stat->read_bytes += (args->trace_file->get_total(TraceOperation::READ) * i);
    stat->write_bytes += (args->trace_file->get_total(TraceOperation::WRITE) * i);
    stat->total_bytes += (stat->read_bytes + stat->write_bytes);

    pthread_exit(NULL);
}

bool BenchSuite::run(const size_t replay_rounds)
{
    std::cout << "DAX area: [" << std::hex << this->mem_area << '-' << (void*) ((uintptr_t) this->mem_area + this->mem_size) << ']' << std::endl;

    const uintptr_t max_offset = reinterpret_cast<uintptr_t>(this->mem_area) + this->mem_size;

    // Calculate the DAX addresses based on the offset inside the trace PMEM region.
    for (TraceEntry &entry : this->trace_file) {
        entry.dax_addr = static_cast<char*>(this->mem_area) + entry.rel_addr;

        if (reinterpret_cast<uintptr_t>(entry.dax_addr) > max_offset) {
            std::cerr << "The following operation exteeds the pre-allocated DAX region by "
                << (static_cast<char*>(entry.dax_addr) - max_offset) << " bytes:" << std::endl;
            std::cerr << entry << std::endl;

            return false;
        }
    }

    // Spawn the threads
    pthread_t threads[this->num_threads] = {};
    struct WorkerArguments thread_args[this->num_threads] = {{(&(this->trace_file)), this->num_samples, replay_rounds, static_cast<size_t>((this->do_cache_warming) ? 1 : 0)}};

    std::cout << "Initializing " << this->num_threads << " threads ..." << std::endl;

    this->drop_caches();

    assert(sysconf(_SC_NPROCESSORS_ONLN) > static_cast<long>(this->num_threads));


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
            return false;
        }
    }

    for (size_t i = 0; i < this->num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

    const struct io_stat* thread_stat = nullptr;

    for (size_t i = 0; i < this->num_threads; ++i) {
        thread_stat = &(thread_args[i].stat);

        std::cout << "Thread " << i << " -> Latency: " << std::dec << thread_stat->latency_sum << " us (" << (static_cast<double>(thread_stat->latency_sum) / 1'000'000'000) << " sec)" <<
            " Read: " << (static_cast<double>(thread_stat->read_bytes) / Mebibyte) << " MB" <<
            " Write: " << (static_cast<double>(thread_stat->write_bytes) / Mebibyte) << " MB" <<
            " Total bytes: "  << (static_cast<double>(thread_stat->total_bytes) / Mebibyte) << " MB"
            " Bandwidth: " << (static_cast<double>(thread_stat->total_bytes) / Gibibyte / (thread_stat->latency_sum / 1'000'000'000)) << " GB/s" << std::endl;

        std::cout << "Collected " << thread_stat->num_collected_samples << " samples!" << std::endl;
    }

    
    for (size_t i = 0; i < this->num_threads; ++i) {
        BenchExport bench_export(thread_args[i]);

        bench_export.export_io_stat("test.csv");
    }
    
    return true;
}

