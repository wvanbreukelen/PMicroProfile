#pragma once

#include <array>
#include <cstddef>
#include <iostream>
#include <linux/perf_event.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <sys/syscall.h>

//#define PMC_VERBOSE

struct iMCProbe {
public:
    size_t num_probes;
    std::array<unsigned int, 16> fd_probes;
};



class PMC {
public:
    PMC() {}

    bool init();
    void print_imcs(std::ostream &os) const;
    
    int add_probe(const int imc_id, const unsigned int event_id) const;
    bool add_imc_probe(const unsigned int event_id, struct iMCProbe &imc_probe) const;
    bool remove_probe(const int fd) const;
    bool remove_imc_probe(const struct iMCProbe& imc_probe) const;

    // inline void probe_reset(const struct iMCProbe& iMCProbe) const;
    // inline void probe_enable(const struct iMCProbe& iMCProbe) const;
    // inline void probe_disable(const struct iMCProbe& iMCProbe) const;

private:
    void find_imcs();

private:
    std::array<unsigned int, 16> imc_ids;
    size_t num_imcs = 0;
};


inline void probe_reset(const struct iMCProbe& iMCProbe)
{
    for (size_t i = 0; i < iMCProbe.num_probes; ++i) {
        (void) ioctl(iMCProbe.fd_probes[i], PERF_EVENT_IOC_RESET, 0);
    }
}

inline void probe_enable(const struct iMCProbe& iMCProbe)
{
    for (size_t i = 0; i < iMCProbe.num_probes; ++i) {
        (void) ioctl(iMCProbe.fd_probes[i], PERF_EVENT_IOC_ENABLE, 0);
    }
}

inline void probe_disable(const struct iMCProbe& iMCProbe)
{
    for (size_t i = 0; i < iMCProbe.num_probes; ++i) {
       	(void) ioctl(iMCProbe.fd_probes[i], PERF_EVENT_IOC_DISABLE, 0);
    }
}

inline void probe_count(const struct iMCProbe& iMCProbe, unsigned long long *count)
{
    long long local_count = 0;

    for (size_t i = 0; i < iMCProbe.num_probes; ++i) {
	    read(iMCProbe.fd_probes[i], &local_count, sizeof(count));
        *(count) += local_count;
    }
}
