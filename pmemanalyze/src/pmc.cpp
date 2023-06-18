#include "pmc.hpp"

#include <filesystem>
#include <regex>
#include <fstream>
#include <cassert>



//#define PMC_VERBOSE


static long perf_event_open(struct perf_event_attr* event_attr, pid_t pid, int cpu,
		     int group_fd, unsigned long flags)
{
    return syscall(__NR_perf_event_open, event_attr, pid, cpu, group_fd, flags);
}

bool PMC::init()
{
    const std::regex pattern("^uncore_imc_[[:xdigit:]]$");
    const std::filesystem::path bus_dir("/sys/bus/event_source/devices");

    size_t i = 0;

    for (const auto &entry : std::filesystem::directory_iterator(bus_dir))
    {
        if (i > this->imc_ids.max_size())
            return false;

        const auto& path = entry.path();
        if (std::filesystem::is_directory(path) && std::regex_match(path.filename().string(), pattern)) {
            const std::filesystem::path& id_file = path / "type";

            std::ifstream file_handle(id_file);

            unsigned int id;
            if (file_handle >> id) {
                this->imc_ids[i] = id;
                ++(this->num_imcs);

                #ifdef PMC_VERBOSE
                std::cout << "Found iMC at: " << path << " with ID " << std::dec << id << std::endl;
                #endif
            }

            ++i;
        }
    }

    return true;
}

void PMC::print_imcs(std::ostream &os) const
{
    for (size_t i = 0; i < num_imcs; ++i)
        os << std::dec << this->imc_ids[i] << " ";
}

int PMC::add_probe(const unsigned int event_id, const int imc_id, const int pid, const unsigned long msr_reg) const
{
    struct perf_event_attr pe;
    int fd;

    memset(&pe, 0, sizeof(struct perf_event_attr));

    pe.type = (imc_id == -1) ? PERF_TYPE_RAW : imc_id;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = event_id;
    pe.sample_type = PERF_SAMPLE_IDENTIFIER;
	
 
    pe.disabled = 1;
    pe.inherit = 1;

    if (imc_id == -1) {
        pe.exclude_guest = 1;

        if (msr_reg)
            pe.bp_addr = msr_reg;
    } else {
	    pe.exclude_guest = 0;
	    pe.exclude_host = 0;
    }

    if (imc_id == -1) {
        fd = perf_event_open(&pe, pid, -1, -1, 0x8);
    } else {
        fd = perf_event_open(&pe, pid, 0, -1, 0);
    }

    if (fd == -1) {
        #ifdef PMC_VERBOSE
        std::cerr << "[iMC " << imc_id << "] Unable to open perf event monitor for event config: 0x" << std::hex << pe.config << " errno: " << std::dec << errno << std::endl;
        std::cout << "config: 0x" << pe.config << " type: 0x" << pe.type << " size: " << pe.size << "sample_type: 0x" << pe.sample_type << "pid: " << pid << std::endl;
        #endif

        return -1;
    } else {
        #ifdef PMC_VERBOSE
        std::cout << "[iMC " << imc_id << "] Added probe for event 0x" << std::hex << event_id << " type " << std::dec << imc_id << std::endl;
        #endif
    }

    int flags = fcntl(fd, F_GETFD);
    flags &= ~FD_CLOEXEC;
    if (fcntl(fd, F_SETFD, flags) < 0) {
	    std::cerr << "error!" << std::endl;
	    return -1;
    }

    return fd;
}

bool PMC::add_imc_probe(const unsigned int event_id, const bool is_single)
{
    struct Probe& probe = this->probes[this->num_probes++];
    probe.set_imc();
    assert(this->num_probes < this->probes.size());

    int fd;

    if (!this->num_imcs)
        return false;

    for (size_t i = 0; i < this->num_imcs; ++i) {
        if ((fd = this->add_probe(event_id, this->imc_ids[i], -1)) < 0) {
            return false;
        }

        ++(probe.num_probes);
        probe.fd_probes[i] = fd;
        #ifdef PMC_VERBOSE
        std::cout << std::dec << fd << " ";
        #endif
    }

    probe.event_id = event_id;

    #ifdef PMC_VERBOSE
    std::cout << "\nNum probes: " << probe.num_probes << std::endl;
    #endif

    return true;
}

bool PMC::add_offcore_probe(const unsigned int event_id, const int pid, const unsigned long msr_reg)
{
    struct Probe& probe = this->probes[this->num_probes++];
    probe.set_oncore();
    assert(this->num_probes < this->probes.size());

    int fd;

    if ((fd = this->add_probe(event_id, -1, pid, msr_reg)) < 0) {
        this->num_probes--;
        return false;
    }

    if (msr_reg)
        probe.msr_reg = msr_reg;

    ++(probe.num_probes);
    probe.fd_probes[0] = fd;
    probe.event_id = event_id;

    assert(probe.num_probes == 1);

    return true;
}

Probe& PMC::get_probe(const unsigned int event_id)
{
    for (size_t i = 0; i < this->num_probes; ++i)
    {
        if (this->probes[i].event_id == event_id)
            return this->probes[i];
    }

    // Just crash the program if we cannot find the probe. Doing runtime checks (std::optional<>) would degrade performance.
    std::cerr << "Error: Unable to get probe for event id " << event_id << ", exiting..." << std::endl;
    assert(false);
    // This is just a placeholder to keep the compiler happy.
    return this->probes[0];
}

Probe& PMC::get_probe_msr(const unsigned int event_id, const unsigned long msr_reg)
{
    for (size_t i = 0; i < this->num_probes; ++i)
    {
        if (this->probes[i].event_id == event_id && this->probes[i].msr_reg == msr_reg)
            return this->probes[i];
    }

    std::cerr << "Error: Unable to get probe for event id " << event_id << ", msr: 0x" << std::hex << msr_reg << std::dec <<  ", exiting..." << std::endl;
    assert(false);
    // This is just a placeholder to keep the compiler happy.
    return this->probes[0];
}

void PMC::enable_imc_probes() const
{
    for (size_t i = 0; i < this->num_probes; ++i)
	if (this->probes[i].is_imc())
        	this->probes[i].probe_enable();
}

void PMC::reset_enable_imc_probes() const
{
    for (size_t i = 0; i < this->num_probes; ++i)
	    if (this->probes[i].is_imc())
            this->probes[i].probe_reset_enable();
}

void PMC::disable_imc_probes() const
{
    for (size_t i = 0; i < this->num_probes; ++i)
	    if (this->probes[i].is_imc())
            this->probes[i].probe_disable();
}

void PMC::reset_imc_probes() const
{
    for (size_t i = 0; i < this->num_probes; ++i)
	    if (this->probes[i].is_imc())
        	this->probes[i].probe_reset();
}

bool PMC::remove_probe(const int fd) const
{
    if (fd < 0)
        return false;

    if (ioctl(fd, PERF_EVENT_IOC_DISABLE, 0) < 0)
        return false;

    if (close(fd) < 0)
        return false;

    return true;
}

bool PMC::remove_imc_probes() const
{
    bool res = true;

    for (size_t z = 0; z < this->num_probes; ++z) {
        for (size_t i = 0; i < this->probes[z].num_probes; ++i) {
            res &= this->remove_probe(this->probes[z].fd_probes[i]);
        }
    }

    return res;
}


