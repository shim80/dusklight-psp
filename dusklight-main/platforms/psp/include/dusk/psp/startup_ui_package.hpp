#ifndef DUSK_PSP_STARTUP_UI_PACKAGE_HPP
#define DUSK_PSP_STARTUP_UI_PACKAGE_HPP

#include <cstdint>

namespace dusk::psp::startup {

enum class UiPackageError : std::uint8_t {
    Ok, NullInput, Truncated, Magic, Version, Size, Crc, Range,
    Record, Duplicate, EdramBudget,
};

struct UiPackageView {
    const std::uint8_t* bytes;
    std::uint32_t size;
    std::uint32_t atlas_width;
    std::uint32_t atlas_height;
    std::uint32_t record_count;
    std::uint32_t records_offset;
    std::uint32_t atlas_offset;
    std::uint32_t atlas_bytes;
};

UiPackageError validate_startup_ui(
    const void* data,
    std::uint32_t size,
    UiPackageView* output);
const char* ui_package_error_name(UiPackageError error);

}  // namespace dusk::psp::startup

#endif
