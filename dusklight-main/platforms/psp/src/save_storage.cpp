#include "dusk/psp/save_storage.hpp"

#include <array>
#include <cstdio>

namespace dusk::psp::save {

StorageResult load_bank_file(const char* path, SaveBank* output) {
    if (path == nullptr || output == nullptr) {
        return StorageResult::ReadError;
    }
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        return StorageResult::NotFound;
    }
    std::array<std::uint8_t, kEncodedBankBytes> bytes = {};
    const std::size_t read = std::fread(bytes.data(), 1, bytes.size(), file);
    const int trailing = std::fgetc(file);
    const int close_result = std::fclose(file);
    if (read != bytes.size() || trailing != EOF || close_result != 0) {
        return StorageResult::ReadError;
    }
    if (decode_bank(bytes.data(), bytes.size(), output) != BankError::Ok) {
        return StorageResult::InvalidBank;
    }
    return StorageResult::Ok;
}

StorageResult store_bank_file(const char* path, const SaveBank& bank) {
    if (path == nullptr) {
        return StorageResult::WriteError;
    }
    std::array<std::uint8_t, kEncodedBankBytes> bytes = {};
    std::size_t written = 0;
    if (encode_bank(bank, bytes.data(), bytes.size(), &written) != BankError::Ok ||
        written != bytes.size()) {
        return StorageResult::WriteError;
    }
    std::FILE* file = std::fopen(path, "wb");
    if (file == nullptr) {
        return StorageResult::WriteError;
    }
    const std::size_t stored = std::fwrite(bytes.data(), 1, bytes.size(), file);
    const int flush_result = std::fflush(file);
    const int close_result = std::fclose(file);
    if (stored != bytes.size() || flush_result != 0 || close_result != 0) {
        return StorageResult::WriteError;
    }
    return StorageResult::Ok;
}

const char* storage_result_name(StorageResult result) {
    switch (result) {
    case StorageResult::Ok: return "ok";
    case StorageResult::NotFound: return "not_found";
    case StorageResult::ReadError: return "read_error";
    case StorageResult::WriteError: return "write_error";
    case StorageResult::InvalidBank: return "invalid_bank";
    }
    return "unknown";
}

}  // namespace dusk::psp::save
