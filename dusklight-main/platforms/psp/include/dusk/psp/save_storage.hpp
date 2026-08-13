#ifndef DUSK_PSP_SAVE_STORAGE_HPP
#define DUSK_PSP_SAVE_STORAGE_HPP

#include "dusk/psp/save_runtime.hpp"

namespace dusk::psp::save {

enum class StorageResult : std::uint8_t {
    Ok = 0,
    NotFound,
    ReadError,
    WriteError,
    InvalidBank,
};

StorageResult load_bank_file(const char* path, SaveBank* output);
StorageResult store_bank_file(const char* path, const SaveBank& bank);
const char* storage_result_name(StorageResult result);

}  // namespace dusk::psp::save

#endif
