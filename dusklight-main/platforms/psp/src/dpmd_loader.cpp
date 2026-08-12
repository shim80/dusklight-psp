#include "dusk/psp/dpmd_loader.hpp"

#include <cstdio>
#include <cstdlib>
#include <limits>

namespace dusk::psp::dpmd {

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
    void* storage = std::malloc(size);
    if (storage == nullptr) {
        std::fclose(file);
        return Error::AllocationFailed;
    }
    const std::size_t read = std::fread(storage, 1, size, file);
    const bool failed = std::ferror(file) != 0;
    const int trailing = std::fgetc(file);
    const int closed = std::fclose(file);
    if (read != size || failed || trailing != EOF || closed != 0) {
        std::free(storage);
        return Error::IoReadFailed;
    }
    PackageView view = {};
    const Error error = validate(storage, size, memory_budget, &view);
    if (error != Error::Ok) {
        std::free(storage);
        return error;
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

}  // namespace dusk::psp::dpmd
