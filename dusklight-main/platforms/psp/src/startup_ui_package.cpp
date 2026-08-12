#include "dusk/psp/startup_ui_package.hpp"

#include <cstring>

namespace dusk::psp::startup {
namespace {

std::uint16_t u16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0]) |
           static_cast<std::uint16_t>(p[1]) << 8;
}

std::uint32_t u32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
           static_cast<std::uint32_t>(p[1]) << 8 |
           static_cast<std::uint32_t>(p[2]) << 16 |
           static_cast<std::uint32_t>(p[3]) << 24;
}

std::uint32_t crc32(const std::uint8_t* bytes, std::uint32_t size) {
    std::uint32_t crc = 0xffffffffu;
    for (std::uint32_t at = 0; at < size; ++at) {
        const std::uint8_t value =
            at >= 12 && at < 16 ? 0 : bytes[at];
        crc ^= value;
        for (std::uint32_t bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^
                  (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

}  // namespace

UiPackageError validate_startup_ui(
    const void* data,
    std::uint32_t size,
    UiPackageView* output) {
    if (data == nullptr || output == nullptr) {
        return UiPackageError::NullInput;
    }
    *output = {};
    if (size < 128) return UiPackageError::Truncated;
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    if (std::memcmp(bytes, "DPSU", 4) != 0) return UiPackageError::Magic;
    if (u16(bytes + 4) != 1 || u16(bytes + 6) != 128) {
        return UiPackageError::Version;
    }
    if (u32(bytes + 8) != size) return UiPackageError::Size;
    if (u32(bytes + 12) != crc32(bytes, size)) {
        return UiPackageError::Crc;
    }
    const std::uint32_t width = u32(bytes + 16);
    const std::uint32_t height = u32(bytes + 20);
    const std::uint32_t count = u32(bytes + 28);
    const std::uint32_t table = u32(bytes + 32);
    const std::uint32_t stride = u32(bytes + 36);
    const std::uint32_t atlas = u32(bytes + 40);
    const std::uint32_t atlas_bytes = u32(bytes + 44);
    if (width == 0 || height == 0 || width > 512 || height > 512 ||
        (width & 7u) != 0 || (height & 7u) != 0 ||
        u32(bytes + 24) != 2 || count == 0 || count > 32 ||
        table != 128 || stride != 32 ||
        table + count * stride > size ||
        atlas < table + count * stride ||
        atlas_bytes != width * height * 2 ||
        atlas_bytes > size - atlas) {
        return UiPackageError::Range;
    }
    if (atlas_bytes > 512u * 512u * 2u) {
        return UiPackageError::EdramBudget;
    }
    bool ids[384] = {};
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::uint8_t* record = bytes + table + index * stride;
        const std::uint16_t id = u16(record);
        const std::uint32_t x = u16(record + 12);
        const std::uint32_t y = u16(record + 14);
        const std::uint32_t w = u16(record + 16);
        const std::uint32_t h = u16(record + 18);
        if (id >= 384 || ids[id] || w == 0 || h == 0 ||
            x > width || w > width - x ||
            y > height || h > height - y ||
            u32(record + 24) == 0) {
            return id < 384 && ids[id]
                ? UiPackageError::Duplicate
                : UiPackageError::Record;
        }
        ids[id] = true;
    }
    *output = {
        bytes, size, width, height, count, table, atlas, atlas_bytes};
    return UiPackageError::Ok;
}

const char* ui_package_error_name(UiPackageError error) {
    static constexpr const char* names[] = {
        "ok", "null_input", "truncated", "magic", "version", "size",
        "crc", "range", "record", "duplicate", "edram_budget"};
    const auto index = static_cast<std::uint8_t>(error);
    return index < sizeof(names) / sizeof(names[0])
        ? names[index] : "unknown";
}

}  // namespace dusk::psp::startup
