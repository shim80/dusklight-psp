#ifndef DUSK_PSP_DPMD_LOADER_HPP
#define DUSK_PSP_DPMD_LOADER_HPP

#include "dusk/psp/dpmd.hpp"

namespace dusk::psp::dpmd {

struct LoadedPackage {
    void* storage;
    std::uint32_t storage_bytes;
    PackageView view;
};

Error load_file(
    const char* path,
    std::uint32_t memory_budget,
    LoadedPackage* output);
void release(LoadedPackage* package);

}  // namespace dusk::psp::dpmd

#endif
