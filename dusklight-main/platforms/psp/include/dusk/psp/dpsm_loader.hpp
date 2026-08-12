#ifndef DUSK_PSP_DPSM_LOADER_HPP
#define DUSK_PSP_DPSM_LOADER_HPP

#include "dusk/psp/dpsm.hpp"

#include <cstdint>

namespace dusk::psp::dpsm {

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

}  // namespace dusk::psp::dpsm

#endif
