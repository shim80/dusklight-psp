#include "dusk/psp/dpsm_loader.hpp"

#include <cstdio>
#include <cstdlib>
#include <limits>

namespace dusk::psp::dpsm {

Error load_file(
    const char* path,
    std::uint32_t memory_budget,
    LoadedPackage* output) {
    if (path == nullptr || path[0] == '\0' || output == nullptr) {
        return Error::NullInput;
    }
    *output = {};

    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        return Error::IoOpenFailed;
    }
    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        return Error::IoSeekFailed;
    }
    const long length = std::ftell(file);
    if (length < 0 ||
        static_cast<unsigned long>(length) >
            std::numeric_limits<std::uint32_t>::max() ||
        std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        return Error::IoSeekFailed;
    }
    const auto size = static_cast<std::uint32_t>(length);
    if (size > kMaximumPackageBytes) {
        std::fclose(file);
        return Error::FileTooLarge;
    }
    if (memory_budget == 0 || size > memory_budget) {
        std::fclose(file);
        return Error::MemoryBudgetExceeded;
    }
    if (size < kHeaderBytes) {
        std::fclose(file);
        return Error::HeaderTruncated;
    }

    void* storage = std::malloc(size);
    if (storage == nullptr) {
        std::fclose(file);
        return Error::AllocationFailed;
    }
    const std::size_t bytes_read = std::fread(storage, 1, size, file);
    const bool failed = std::ferror(file) != 0;
    const int trailing = std::fgetc(file);
    const int close_result = std::fclose(file);
    if (bytes_read != size || failed || trailing != EOF || close_result != 0) {
        std::free(storage);
        return Error::IoReadFailed;
    }

    PackageView view = {};
    const Error validation = validate(storage, size, memory_budget, &view);
    if (validation != Error::Ok) {
        std::free(storage);
        return validation;
    }
    output->storage = storage;
    output->storage_bytes = size;
    output->view = view;
    return Error::Ok;
}

void release(LoadedPackage* package) {
    if (package == nullptr) {
        return;
    }
    std::free(package->storage);
    *package = {};
}

}  // namespace dusk::psp::dpsm
