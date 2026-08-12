#ifndef DUSK_PSP_COLOR_PACKING_HPP
#define DUSK_PSP_COLOR_PACKING_HPP

#include <cstdint>

namespace dusk::psp::color {

struct GxColorRgba8 {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
    std::uint8_t alpha;
};

struct PackedArgb32 {
    std::uint32_t value;
};

struct PspColorAbgr8888 {
    std::uint32_t value;
};

constexpr PackedArgb32 pack_argb(GxColorRgba8 color) {
    return {
        (static_cast<std::uint32_t>(color.alpha) << 24) |
        (static_cast<std::uint32_t>(color.red) << 16) |
        (static_cast<std::uint32_t>(color.green) << 8) |
        static_cast<std::uint32_t>(color.blue)};
}

constexpr GxColorRgba8 unpack_argb(PackedArgb32 color) {
    return {
        static_cast<std::uint8_t>(color.value >> 16),
        static_cast<std::uint8_t>(color.value >> 8),
        static_cast<std::uint8_t>(color.value),
        static_cast<std::uint8_t>(color.value >> 24),
    };
}

constexpr PspColorAbgr8888 to_psp_abgr(GxColorRgba8 color) {
    return {
        (static_cast<std::uint32_t>(color.alpha) << 24) |
        (static_cast<std::uint32_t>(color.blue) << 16) |
        (static_cast<std::uint32_t>(color.green) << 8) |
        static_cast<std::uint32_t>(color.red)};
}

constexpr PspColorAbgr8888 to_psp_abgr(PackedArgb32 color) {
    return to_psp_abgr(unpack_argb(color));
}

constexpr GxColorRgba8 unpack_psp_abgr(PspColorAbgr8888 color) {
    return {
        static_cast<std::uint8_t>(color.value),
        static_cast<std::uint8_t>(color.value >> 8),
        static_cast<std::uint8_t>(color.value >> 16),
        static_cast<std::uint8_t>(color.value >> 24),
    };
}

constexpr std::uint8_t psp_memory_byte(
    PspColorAbgr8888 color, std::uint32_t index) {
    return static_cast<std::uint8_t>(
        color.value >> ((index & 3u) * 8u));
}

}  // namespace dusk::psp::color

#endif
