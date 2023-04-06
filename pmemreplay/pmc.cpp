#include "pmc.hpp"

#include <filesystem>
#include <regex>
#include <fstream>



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

int PMC::add_probe(const int imc_id, const unsigned int event_id) const
{
    struct perf_event_attr pe;
    int fd;

    memset(&pe, 0, sizeof(struct perf_event_attr));

    pe.type = imc_id;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = event_id;
    pe.sample_type = PERF_SAMPLE_IDENTIFIER;
   // pe.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
    pe.disabled = 1;
    pe.inherit = 1;
    pe.exclude_guest = 0;
    pe.exclude_host = 0;

    fd = perf_event_open(&pe, -1, 0, -1, 0);

    if (fd == -1) {
        #ifdef PMC_VERBOSE
        std::cerr << "[iMC " << imc_id << "] Unable to open perf event monitor for event config: 0x" << std::hex << pe.config << " errno: " << std::dec << errno << std::endl;
        #endif

        return -1;
        //exit(EXIT_FAILURE);
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

bool PMC::add_imc_probe(const unsigned int event_id, struct iMCProbe &imc_probe) const
{
    int fd;

    for (size_t i = 0; i < this->num_imcs; ++i) {
        if ((fd = this->add_probe(this->imc_ids[i], event_id)) < 0) {
            return false;
        }

        ++(imc_probe.num_probes);
        imc_probe.fd_probes[i] = fd;
	#ifdef PMC_VERBOSE
	std::cout << std::dec << fd << " ";
	#endif
    }

    #ifdef PMC_VERBOSE
    std::cout << "\nNum probes: " << imc_probe.num_probes << std::endl;
    #endif

    return true;
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

bool PMC::remove_imc_probe(const iMCProbe& imc_probe) const
{
    bool res = true;

    for (size_t i = 0; i < imc_probe.num_probes; ++i) {
        res &= this->remove_probe(imc_probe.fd_probes[i]);
    }

    return res;
}


