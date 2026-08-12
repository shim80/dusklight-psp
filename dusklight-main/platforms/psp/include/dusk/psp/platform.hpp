#ifndef DUSK_PSP_PLATFORM_HPP
#define DUSK_PSP_PLATFORM_HPP

#include <cstdint>

namespace dusk::psp {

struct RuntimeConfig {
    const char* application_name;
    const char* game_directory;
};

struct MemorySnapshot {
    std::uint32_t free_bytes;
    std::uint32_t largest_block_bytes;
};

bool initialize(const RuntimeConfig& config);
void shutdown();

void log(const char* message);
void log_error(const char* message, int error_code);

std::uint64_t monotonic_microseconds();
void sleep_microseconds(std::uint32_t duration);

bool make_game_path(const char* leaf, char* output, std::uint32_t capacity);
bool make_game_relative_path(
    const char* relative_path, char* output, std::uint32_t capacity);
bool write_file(const char* path, const void* data, std::uint32_t size);
bool read_file(const char* path, void* data, std::uint32_t capacity,
               std::uint32_t* size_read);

MemorySnapshot memory_snapshot();
int last_error();
const char* last_error_message();

}  // namespace dusk::psp

#endif
