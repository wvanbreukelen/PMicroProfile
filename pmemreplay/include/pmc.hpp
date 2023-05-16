#pragma once

#include <array>
#include <cstddef>
#include <iostream>
#include <linux/perf_event.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cassert>

#include <sys/syscall.h>

//#define PMC_VERBOSE


struct Probe {
public:
    bool is_imc;
    size_t num_probes;
    std::array<unsigned int, 16> fd_probes;
    unsigned int event_id = 0;

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
        is_imc = true;
    }

    inline void set_oncore() {
        is_imc = false;
    }
};

class PMC {
public:
    PMC() {}

    bool init();
    void print_imcs(std::ostream &os) const;
    
    bool add_imc_probe(const unsigned int event_id);
    bool add_oncore_probe(const unsigned int event_id, const int pid);
    
    void enable_probes() const;
    void disable_probes() const;
    void reset_probes() const;

    bool remove_probe(const int fd) const;
    bool remove_imc_probes() const;
    Probe& get_probe(const unsigned int event_id);

    // inline void probe_reset(const struct iMCProbe& iMCProbe) const;
    // inline void probe_enable(const struct iMCProbe& iMCProbe) const;
    // inline void probe_disable(const struct iMCProbe& iMCProbe) const;

private:
    int add_probe(const unsigned int event_id, const int imc_id, const int pid) const;
    void find_imcs();

private:
    std::array<unsigned int, 16> imc_ids;
    std::array<Probe, 32> probes = {};

    size_t num_imcs = 0;
    size_t num_probes = 0;
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
