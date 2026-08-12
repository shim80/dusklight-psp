#ifndef DUSK_PSP_GRAPHICS_HPP
#define DUSK_PSP_GRAPHICS_HPP

#include <cstdint>

namespace dusk::psp::graphics {

inline constexpr std::uint32_t kVisibleWidth = 480;
inline constexpr std::uint32_t kVisibleHeight = 272;
inline constexpr std::uint32_t kBufferStride = 512;
inline constexpr std::uint32_t kFramebufferBytesPerPixel = 4;
inline constexpr std::uint32_t kDepthBytesPerPixel = 2;
inline constexpr std::uint32_t kCommandListBytes = 64 * 1024;
inline constexpr std::uint32_t kTextureWidth = 32;
inline constexpr std::uint32_t kTextureHeight = 32;
inline constexpr std::uint32_t kMaximumPixelReadbacks = 8;

constexpr std::uint32_t pack_rgba8(std::uint8_t red, std::uint8_t green,
                                   std::uint8_t blue, std::uint8_t alpha) {
    return static_cast<std::uint32_t>(red) |
           (static_cast<std::uint32_t>(green) << 8) |
           (static_cast<std::uint32_t>(blue) << 16) |
           (static_cast<std::uint32_t>(alpha) << 24);
}

inline constexpr std::uint32_t kBackgroundColor =
    pack_rgba8(16, 32, 48, 255);
inline constexpr std::uint32_t kTriangleColor =
    pack_rgba8(224, 48, 48, 255);
inline constexpr std::uint32_t kCheckerColorA =
    pack_rgba8(240, 208, 32, 255);
inline constexpr std::uint32_t kCheckerColorB =
    pack_rgba8(32, 96, 224, 255);

struct VramLayout {
    std::uint32_t available_bytes;
    std::uint32_t reserved_bytes;
    std::uint32_t remaining_bytes;
    std::uint32_t draw_buffer_offset;
    std::uint32_t display_buffer_offset;
    std::uint32_t depth_buffer_offset;
    std::uint32_t framebuffer_bytes;
    std::uint32_t depth_buffer_bytes;
    bool valid;
};

constexpr VramLayout compute_vram_layout(std::uint32_t available_bytes) {
    constexpr std::uint32_t framebuffer_bytes =
        kBufferStride * kVisibleHeight * kFramebufferBytesPerPixel;
    constexpr std::uint32_t depth_buffer_bytes =
        kBufferStride * kVisibleHeight * kDepthBytesPerPixel;
    constexpr std::uint32_t reserved_bytes =
        framebuffer_bytes * 2 + depth_buffer_bytes;
    return {
        available_bytes,
        reserved_bytes,
        available_bytes >= reserved_bytes
            ? available_bytes - reserved_bytes
            : 0,
        0,
        framebuffer_bytes,
        framebuffer_bytes * 2,
        framebuffer_bytes,
        depth_buffer_bytes,
        available_bytes >= reserved_bytes,
    };
}

struct PixelCheck {
    std::uint16_t x;
    std::uint16_t y;
    std::uint32_t expected;
    std::uint32_t actual;
    bool matches;
};

struct PixelRequest {
    std::uint16_t x;
    std::uint16_t y;
    std::uint32_t expected;
};

struct PixelReadbackMetrics {
    std::uint32_t cache_writeback_operations;
    std::uint32_t cache_invalidate_operations;
    bool guard_regions_valid;
};

struct PixelVerification {
    PixelCheck background;
    PixelCheck triangle;
    PixelCheck checker_a;
    PixelCheck checker_b;
    bool all_match;
};

struct GraphicsMetrics {
    VramLayout vram;
    std::uint32_t frames_rendered;
    std::uint32_t command_list_bytes;
    std::uint32_t command_list_bytes_used;
    bool synchronized;
    bool initialized;
};

bool initialize(void* command_list, std::uint32_t command_list_bytes);
void shutdown();

bool begin_frame();
bool draw_test_pattern();
bool end_frame();
bool synchronize();
bool read_pixels(
    const PixelRequest* requests,
    std::uint32_t count,
    PixelCheck* checks,
    PixelReadbackMetrics* readback_metrics);
bool verify_pixels(PixelVerification* verification);
bool swap_buffers();

GraphicsMetrics metrics();
int last_error();
const char* last_error_message();

}  // namespace dusk::psp::graphics

#endif
