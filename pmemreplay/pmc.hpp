#pragma once

#include <array>
#include <cstddef>
#include <iostream>

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
    
    int add_probe(const unsigned int imc_id, const unsigned int event_id) const;
    bool add_imc_probe(const unsigned int event_id, struct iMCProbe &imc_probe) const;
    bool remove_probe(const int fd) const;
    bool remove_imc_probe(const struct iMCProbe& imc_probe) const;

private:
    void find_imcs();

private:
    std::array<unsigned int, 16> imc_ids;
    size_t num_imcs = 0;
};