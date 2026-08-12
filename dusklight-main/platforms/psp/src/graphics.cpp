#include "dusk/psp/graphics.hpp"

#include <pspdisplay.h>
#include <pspge.h>
#include <pspgu.h>
#include <psputils.h>

#include <cstddef>
#include <cstdint>

namespace dusk::psp::graphics {
namespace {

enum ErrorCode {
    kSuccess = 0,
    kInvalidConfiguration = 1,
    kInsufficientVram = 2,
    kGuInitializationFailed = 3,
    kInvalidFrameState = 4,
    kGuListFailed = 5,
    kGuSynchronizationFailed = 6,
    kPixelVerificationFailed = 7,
};

struct ColorVertex {
    std::uint32_t color;
    std::int16_t x;
    std::int16_t y;
    std::int16_t z;
};

struct TextureVertex {
    std::int16_t u;
    std::int16_t v;
    std::int16_t x;
    std::int16_t y;
    std::int16_t z;
};

static_assert(sizeof(ColorVertex) == 12);
static_assert(sizeof(TextureVertex) == 10);

alignas(16) std::uint32_t g_texture[kTextureWidth * kTextureHeight] = {};
struct alignas(16) PixelReadbackStorage {
    alignas(16) std::uint32_t before[4];
    alignas(16)
        std::uint32_t rows[kMaximumPixelReadbacks][16];
    alignas(16) std::uint32_t after[4];
};

PixelReadbackStorage g_pixel_readback = {};
alignas(16) const ColorVertex g_triangle[3] = {
    {kTriangleColor, 48, 48, 0},
    {kTriangleColor, 224, 48, 0},
    {kTriangleColor, 136, 208, 0},
};
alignas(16) const TextureVertex g_quad[4] = {
    {0, 0, 288, 64, 0},
    {32, 0, 448, 64, 0},
    {0, 32, 288, 224, 0},
    {32, 32, 448, 224, 0},
};

GraphicsMetrics g_metrics = {};
void* g_command_list = nullptr;
void* g_edram_base = nullptr;
std::uint32_t g_render_offset = 0;
std::uint32_t g_completed_frame_offset = 0;
int g_last_error = kSuccess;
const char* g_last_error_message = "";
bool g_gu_initialized = false;
bool g_frame_active = false;
bool g_frame_finished = false;

constexpr std::uint32_t kReadbackGuardBefore = 0xA55AA55Au;
constexpr std::uint32_t kReadbackGuardAfter = 0x5AA55AA5u;

void set_error(int code, const char* message) {
    g_last_error = code;
    g_last_error_message = message;
}

void* relative_vram_pointer(std::uint32_t offset) {
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(offset));
}

void generate_checker_texture() {
    for (std::uint32_t y = 0; y < kTextureHeight; ++y) {
        for (std::uint32_t x = 0; x < kTextureWidth; ++x) {
            const bool alternate = ((x / 8) + (y / 8)) % 2 != 0;
            g_texture[y * kTextureWidth + x] =
                alternate ? kCheckerColorB : kCheckerColorA;
        }
    }
}

PixelCheck checked_pixel(
    std::uint16_t x,
    std::uint16_t y,
    std::uint32_t expected,
    std::uint32_t actual) {
    constexpr std::uint32_t kDisplayRgbMask = 0x00FFFFFFu;
    const std::uint32_t expected_rgb = expected & kDisplayRgbMask;
    const std::uint32_t actual_rgb = actual & kDisplayRgbMask;
    return {x, y, expected_rgb, actual_rgb, actual_rgb == expected_rgb};
}

void initialize_readback_guards() {
    for (std::uint32_t& value : g_pixel_readback.before) {
        value = kReadbackGuardBefore;
    }
    for (std::uint32_t& value : g_pixel_readback.after) {
        value = kReadbackGuardAfter;
    }
}

bool readback_guards_valid() {
    for (std::uint32_t value : g_pixel_readback.before) {
        if (value != kReadbackGuardBefore) {
            return false;
        }
    }
    for (std::uint32_t value : g_pixel_readback.after) {
        if (value != kReadbackGuardAfter) {
            return false;
        }
    }
    return true;
}

bool readback_sample_rows(
    const PixelRequest* requests,
    std::uint32_t count,
    PixelReadbackMetrics* readback_metrics) {
    sceKernelDcacheWritebackInvalidateRange(
        g_pixel_readback.rows,
        count * sizeof(g_pixel_readback.rows[0]));
    ++readback_metrics->cache_writeback_operations;
    ++readback_metrics->cache_invalidate_operations;
    if (sceGuStart(GU_DIRECT, g_command_list) < 0) {
        set_error(kGuListFailed, "unable to start GU readback list");
        return false;
    }

    auto* completed_frame = static_cast<std::uint8_t*>(g_edram_base) +
                            g_completed_frame_offset;
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::uint16_t source_x =
            static_cast<std::uint16_t>(requests[index].x & ~0xFu);
        sceGuCopyImage(
            GU_PSM_8888,
            source_x,
            requests[index].y,
            16,
            1,
            kBufferStride,
            completed_frame,
            0,
            0,
            16,
            g_pixel_readback.rows[index]);
    }
    sceGuTexSync();
    if (sceGuFinish() < 0) {
        set_error(kGuListFailed, "sceGuFinish failed for pixel readback");
        return false;
    }
    if (sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE) < 0) {
        set_error(kGuSynchronizationFailed,
                  "GU pixel readback did not synchronize");
        return false;
    }
    sceKernelDcacheInvalidateRange(
        g_pixel_readback.rows,
        count * sizeof(g_pixel_readback.rows[0]));
    ++readback_metrics->cache_invalidate_operations;
    readback_metrics->guard_regions_valid = readback_guards_valid();
    if (!readback_metrics->guard_regions_valid) {
        set_error(kPixelVerificationFailed, "pixel readback guard changed");
        return false;
    }
    return true;
}

}  // namespace

bool initialize(void* command_list, std::uint32_t command_list_bytes) {
    if (g_gu_initialized) {
        return true;
    }
    g_last_error = kSuccess;
    g_last_error_message = "";
    g_metrics = {};

    if (command_list == nullptr ||
        (reinterpret_cast<std::uintptr_t>(command_list) & 0xF) != 0 ||
        command_list_bytes < kCommandListBytes) {
        set_error(kInvalidConfiguration,
                  "GU command list is missing, undersized, or misaligned");
        return false;
    }

    g_edram_base = sceGeEdramGetAddr();
    g_metrics.vram = compute_vram_layout(sceGeEdramGetSize());
    g_metrics.command_list_bytes = command_list_bytes;
    if (g_edram_base == nullptr || !g_metrics.vram.valid) {
        set_error(kInsufficientVram,
                  "reported EDRAM cannot hold two framebuffers and depth");
        return false;
    }

    g_command_list = command_list;
    g_render_offset = g_metrics.vram.draw_buffer_offset;
    g_completed_frame_offset = g_render_offset;
    initialize_readback_guards();
    generate_checker_texture();
    sceKernelDcacheWritebackAll();

    if (sceGuInit() < 0) {
        set_error(kGuInitializationFailed, "sceGuInit failed");
        return false;
    }
    g_gu_initialized = true;

    if (sceGuStart(GU_DIRECT, g_command_list) < 0) {
        set_error(kGuListFailed, "unable to start GU setup list");
        shutdown();
        return false;
    }
    sceGuDrawBuffer(
        GU_PSM_8888,
        relative_vram_pointer(g_metrics.vram.draw_buffer_offset),
        kBufferStride);
    sceGuDispBuffer(
        kVisibleWidth,
        kVisibleHeight,
        relative_vram_pointer(g_metrics.vram.display_buffer_offset),
        kBufferStride);
    sceGuDepthBuffer(
        relative_vram_pointer(g_metrics.vram.depth_buffer_offset),
        kBufferStride);
    sceGuOffset(2048 - (kVisibleWidth / 2), 2048 - (kVisibleHeight / 2));
    sceGuViewport(2048, 2048, kVisibleWidth, kVisibleHeight);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, kVisibleWidth, kVisibleHeight);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_STENCIL_TEST);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_DITHER);
    sceGuDisable(GU_FOG);
    sceGuDisable(GU_LIGHTING);
    sceGuDisable(GU_COLOR_LOGIC_OP);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuShadeModel(GU_FLAT);
    if (sceGuFinish() < 0 ||
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE) < 0) {
        set_error(kGuSynchronizationFailed,
                  "GU setup list did not synchronize");
        shutdown();
        return false;
    }

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
    g_metrics.initialized = true;
    return true;
}

void shutdown() {
    if (!g_gu_initialized) {
        return;
    }
    if (g_frame_active) {
        sceGuFinish();
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
        g_frame_active = false;
    }
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
    g_gu_initialized = false;
    g_frame_finished = false;
    g_metrics.initialized = false;
}

bool begin_frame() {
    if (!g_gu_initialized || g_frame_active) {
        set_error(kInvalidFrameState, "begin_frame called in invalid state");
        return false;
    }
    if (sceGuStart(GU_DIRECT, g_command_list) < 0) {
        set_error(kGuListFailed, "unable to start GU frame list");
        return false;
    }
    g_frame_active = true;
    g_frame_finished = false;
    g_metrics.synchronized = false;

    sceGuDisable(GU_TEXTURE_2D);
    sceGuClearColor(kBackgroundColor);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    return true;
}

bool draw_test_pattern() {
    if (!g_frame_active) {
        set_error(kInvalidFrameState,
                  "draw_test_pattern requires an active frame");
        return false;
    }

    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(
        GU_TRIANGLES,
        GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
        3,
        nullptr,
        g_triangle);

    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, GU_FALSE);
    sceGuTexImage(
        0,
        kTextureWidth,
        kTextureHeight,
        kTextureWidth,
        g_texture);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuDrawArray(
        GU_TRIANGLE_STRIP,
        GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
        4,
        nullptr,
        g_quad);
    sceGuDisable(GU_TEXTURE_2D);
    return true;
}

bool end_frame() {
    if (!g_frame_active) {
        set_error(kInvalidFrameState, "end_frame called without active frame");
        return false;
    }
    const int command_list_bytes = sceGuFinish();
    if (command_list_bytes < 0) {
        set_error(kGuListFailed, "sceGuFinish failed");
        g_frame_active = false;
        return false;
    }
    g_metrics.command_list_bytes_used =
        static_cast<std::uint32_t>(command_list_bytes);
    if (g_metrics.command_list_bytes_used >
        g_metrics.command_list_bytes) {
        set_error(kGuListFailed, "GU command list exceeded its capacity");
        g_frame_active = false;
        return false;
    }
    g_frame_active = false;
    g_frame_finished = true;
    return true;
}

bool synchronize() {
    if (!g_gu_initialized || !g_frame_finished) {
        set_error(kInvalidFrameState,
                  "synchronize called before a finished frame");
        return false;
    }
    if (sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE) < 0) {
        set_error(kGuSynchronizationFailed, "sceGuSync failed");
        return false;
    }
    g_metrics.synchronized = true;
    g_completed_frame_offset = g_render_offset;
    ++g_metrics.frames_rendered;
    return true;
}

bool verify_pixels(PixelVerification* verification) {
    if (verification == nullptr || !g_metrics.synchronized) {
        set_error(kInvalidFrameState,
                  "pixel verification requires synchronized output");
        return false;
    }

    constexpr PixelRequest requests[4] = {
        {470, 250, kBackgroundColor},
        {136, 100, kTriangleColor},
        {308, 84, kCheckerColorA},
        {348, 84, kCheckerColorB},
    };
    PixelCheck checks[4] = {};
    PixelReadbackMetrics readback_metrics = {};
    if (!read_pixels(
            requests, 4, checks, &readback_metrics)) {
        return false;
    }
    verification->background = checks[0];
    verification->triangle = checks[1];
    verification->checker_a = checks[2];
    verification->checker_b = checks[3];
    verification->all_match =
        verification->background.matches &&
        verification->triangle.matches &&
        verification->checker_a.matches &&
        verification->checker_b.matches;
    if (!verification->all_match) {
        set_error(kPixelVerificationFailed,
                  "one or more deterministic pixels differ");
        return false;
    }
    return true;
}

bool read_pixels(
    const PixelRequest* requests,
    std::uint32_t count,
    PixelCheck* checks,
    PixelReadbackMetrics* readback_metrics) {
    if (requests == nullptr ||
        checks == nullptr ||
        readback_metrics == nullptr ||
        count == 0 ||
        count > kMaximumPixelReadbacks ||
        !g_metrics.synchronized) {
        set_error(kInvalidFrameState, "invalid pixel readback request");
        return false;
    }
    *readback_metrics = {};
    for (std::uint32_t index = 0; index < count; ++index) {
        if (requests[index].x >= kVisibleWidth ||
            requests[index].y >= kVisibleHeight) {
            set_error(kInvalidConfiguration, "pixel request is out of bounds");
            return false;
        }
    }
    if (!readback_sample_rows(requests, count, readback_metrics)) {
        return false;
    }

    bool all_match = true;
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::uint32_t row_offset = requests[index].x & 0xFu;
        checks[index] = checked_pixel(
            requests[index].x,
            requests[index].y,
            requests[index].expected,
            g_pixel_readback.rows[index][row_offset]);
        all_match = all_match && checks[index].matches;
    }
    if (!all_match) {
        set_error(kPixelVerificationFailed,
                  "one or more deterministic pixels differ");
        return false;
    }
    return true;
}

bool swap_buffers() {
    if (!g_gu_initialized || !g_metrics.synchronized) {
        set_error(kInvalidFrameState,
                  "swap_buffers requires synchronized output");
        return false;
    }
    sceDisplayWaitVblankStart();
    const void* new_render_buffer = sceGuSwapBuffers();
    g_render_offset = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(new_render_buffer));
    return true;
}

GraphicsMetrics metrics() {
    return g_metrics;
}

int last_error() {
    return g_last_error;
}

const char* last_error_message() {
    return g_last_error_message;
}

}  // namespace dusk::psp::graphics
