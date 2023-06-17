#pragma once

#include <array>
#include <cstddef>
#include <iostream>
#include <linux/perf_event.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cassert>
#include <sys/mman.h>

#include <sys/syscall.h>

//#define PMC_VERBOSE


struct Probe {
public:
    bool _is_imc;
    size_t num_probes;
    std::array<unsigned int, 16> fd_probes;
    unsigned int event_id = 0;
    unsigned long msr_reg = 0x0;
    struct perf_event_mmap_page* mmap_buf = nullptr;
    size_t mmap_size = 0;

    inline void probe_reset() const
    {
        for (size_t i = 0; i < this->num_probes; ++i)
            (void) ioctl(this->fd_probes[i], PERF_EVENT_IOC_RESET, 0);
    }

    inline void probe_enable() const
    {
        for (size_t i = 0; i < this->num_probes; ++i)
       	    (void) ioctl(this->fd_probes[i], PERF_EVENT_IOC_ENABLE, 0);
    }

    inline void probe_disable() const
    {
        for (size_t i = 0; i < this->num_probes; ++i)
       	    (void) ioctl(this->fd_probes[i], PERF_EVENT_IOC_DISABLE, 0);
    }

    inline void probe_count(unsigned long long *count) const
    {
        ssize_t bytes_read;
        long long local_count = 0;

        for (size_t i = 0; i < this->num_probes; ++i) {
            bytes_read = read(this->fd_probes[i], &local_count, sizeof(count));
            *(count) += local_count;
        }
    }

    inline void probe_count_single(unsigned long long *count) const
    {
        ssize_t bytes_read;
        if (this->num_probes > 0)
            bytes_read = read(this->fd_probes[0], count, sizeof(count));

	if (bytes_read != sizeof(count))
		assert(false);
	//std::cout << "Bytes read: " << bytes_read << std::endl;
    }

    inline void set_imc() {
        _is_imc = true;
    }

    inline void set_oncore() {
        _is_imc = false;
    }

    inline bool is_imc() const {
	    return _is_imc;
    }

    ~Probe() {
        if (mmap_buf) {
            munmap(mmap_buf, mmap_size);
            mmap_buf = nullptr;
            mmap_size = 0;
        }
    }
};

class PMC {
public:
    PMC() {}

    bool init();
    void print_imcs(std::ostream &os) const;
    
    bool add_imc_probe(const unsigned int event_id, const bool is_single = false);
    bool add_offcore_probe(const unsigned int event_id, const int pid, const unsigned long msr = 0x0);
    
    void enable_imc_probes() const;
    void disable_imc_probes() const;
    void reset_imc_probes() const;

    bool remove_probe(const int fd) const;
    bool remove_imc_probes() const;
    Probe& get_probe(const unsigned int event_id);
    Probe& get_probe_msr(const unsigned int event_id, const unsigned long msr_reg);
    
    void read_samples();

    // inline void probe_reset(const struct iMCProbe& iMCProbe) const;
    // inline void probe_enable(const struct iMCProbe& iMCProbe) const;
    // inline void probe_disable(const struct iMCProbe& iMCProbe) const;

private:
    int add_probe(const unsigned int event_id, const int imc_id, const int pid, const unsigned long msr_reg = 0x0) const;
    void find_imcs();

    int read_samples_probe(Probe &p);
    int read_mmap_buffer(struct perf_event_mmap_page *mmap_buf, char *out, size_t sz);
    void skip_mmap_buffer(struct perf_event_mmap_page *mmap_buf, size_t sz);
    void process_lost_sample(struct perf_event_mmap_page *mmap_buf);
    void process_exit_sample(struct perf_event_mmap_page *mmap_buf);
    void process_freq_sample(struct perf_event_mmap_page *mmap_buf);
    int process_single_sample(struct perf_event_mmap_page *mmap_buf);

private:
    std::array<unsigned int, 16> imc_ids;
    std::array<Probe, 32> probes = {};

    size_t num_imcs = 0;
    size_t num_probes = 0;
    size_t mmap_size = 0;
    size_t pgmsk = 0;
};

struct perf_event_sample 
{
    //struct perf_event_header header;
    uint64_t   sample_id;           /* if PERF_SAMPLE_IDENTIFIER */
    uint64_t   ip;                  /* if PERF_SAMPLE_IP */
    uint32_t   pid, tid;            /* if PERF_SAMPLE_TID */
    uint64_t   time;                /* if PERF_SAMPLE_TIME */
    uint64_t   addr;                /* if PERF_SAMPLE_ADDR */
    uint64_t   id;                  /* if PERF_SAMPLE_ID */
    uint64_t   stream_id;           /* if PERF_SAMPLE_STREAM_ID */
    uint32_t   cpu, res;            /* if PERF_SAMPLE_CPU */
    uint64_t   period;              /* if PERF_SAMPLE_PERIOD */
    //struct read_format v;         /* if PERF_SAMPLE_READ */
    uint64_t   nr;                  /* if PERF_SAMPLE_CALLCHAIN */
    uint64_t  *ips;                 /* if PERF_SAMPLE_CALLCHAIN */
    uint32_t   raw_size;            /* if PERF_SAMPLE_RAW */
    char      *raw_data;            /* if PERF_SAMPLE_RAW */
    uint64_t   bnr;                 /* if PERF_SAMPLE_BRANCH_STACK */
    //struct perf_branch_entry *lbr; /* if PERF_SAMPLE_BRANCH_STACK */
    uint64_t   abi;                 /* if PERF_SAMPLE_REGS_USER */
    uint64_t  *regs;                /* if PERF_SAMPLE_REGS_USER */
    uint64_t   stack_size;          /* if PERF_SAMPLE_STACK_USER */
    char      *stack_data;          /* if PERF_SAMPLE_STACK_USER */
    uint64_t   dyn_size;            /* if PERF_SAMPLE_STACK_USER */
    uint64_t   weight;              /* if PERF_SAMPLE_WEIGHT */
    uint64_t   data_src;            /* if PERF_SAMPLE_DATA_SRC */
    uint64_t   transaction;         /* if PERF_SAMPLE_TRANSACTION */

    size_t data_size;
    size_t num_dims;
    size_t access_index[3];
    const char *data_symbol;
};



// inline void probe_reset(const struct iMCProbe& iMCProbe)
// {
//     for (size_t i = 0; i < iMCProbe.num_probes; ++i) {
//         (void) ioctl(iMCProbe.fd_probes[i], PERF_EVENT_IOC_RESET, 0);
//     }
// }

// inline void probe_enable(const struct iMCProbe& iMCProbe)
// {
//     for (size_t i = 0; i < iMCProbe.num_probes; ++i) {
//         (void) ioctl(iMCProbe.fd_probes[i], PERF_EVENT_IOC_ENABLE, 0);
//     }
// }

// inline void probe_disable(const struct iMCProbe& iMCProbe)
// {
//     for (size_t i = 0; i < iMCProbe.num_probes; ++i) {
//        	(void) ioctl(iMCProbe.fd_probes[i], PERF_EVENT_IOC_DISABLE, 0);
//     }
// }

// inline void probe_count(const struct iMCProbe& iMCProbe, unsigned long long *count)
// {
//     long long local_count = 0;

//     for (size_t i = 0; i < iMCProbe.num_probes; ++i) {
// 	    read(iMCProbe.fd_probes[i], &local_count, sizeof(count));
//         *(count) += local_count;
//     }
// }

// inline void probe_count_single_imc(const struct iMCProbe& iMCProbe, unsigned long long *count)
// {
//     if (iMCProbe.num_probes > 0)
//         read(iMCProbe.fd_probes[0], count, sizeof(count));
// }
