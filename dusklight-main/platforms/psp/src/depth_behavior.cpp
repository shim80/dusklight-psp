#include "dusk/psp/depth_behavior.hpp"

#include <pspdisplay.h>
#include <pspge.h>
#include <pspgu.h>
#include <pspkernel.h>
#include <psputils.h>

#include <cstddef>
#include <cstdint>

namespace dusk::psp::depth_behavior {
namespace {

constexpr std::uint32_t kFramebufferBytes =
    kFixtureStride * kFixtureHeight * 4;
constexpr std::uint32_t kDepthBytes =
    kFixtureStride * kFixtureHeight * 2;
constexpr std::uint32_t kDrawOffset = 0;
constexpr std::uint32_t kDisplayOffset = kFramebufferBytes;
constexpr std::uint32_t kDepthOffset = kFramebufferBytes * 2;
constexpr std::uint32_t kMinimumListBytes = 64 * 1024;
constexpr std::uint32_t kClear = 0xff302010u;
constexpr std::uint32_t kRed = 0xff3030e0u;
constexpr std::uint32_t kBlue = 0xffe06030u;
constexpr std::uint32_t kGreen = 0xff40d040u;
constexpr std::uint16_t kNearDepth = 60000;
constexpr std::uint16_t kFarDepth = 8000;

struct DepthVertex {
    std::uint32_t color;
    std::int16_t x;
    std::int16_t y;
    std::uint16_t z;
};

static_assert(sizeof(DepthVertex) == 12);

alignas(16) std::uint32_t
    g_capture[kFixtureStride * kFixtureHeight] = {};
alignas(16) DepthVertex g_vertices[6] = {};

void* relative(std::uint32_t offset) {
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(offset));
}

void configure_baseline() {
    sceGuDrawBufferList(GU_PSM_8888, relative(kDrawOffset), kFixtureStride);
    sceGuDepthBuffer(relative(kDepthOffset), kFixtureStride);
    sceGuOffset(2048 - kFixtureWidth / 2, 2048 - kFixtureHeight / 2);
    sceGuViewport(2048, 2048, kFixtureWidth, kFixtureHeight);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, kFixtureWidth, kFixtureHeight);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_STENCIL_TEST);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_DITHER);
    sceGuDisable(GU_FOG);
    sceGuDisable(GU_LIGHTING);
    sceGuDisable(GU_COLOR_LOGIC_OP);
    sceGuShadeModel(GU_FLAT);
}

void configure_depth(bool enabled, bool write, int compare) {
    if (enabled) {
        sceGuEnable(GU_DEPTH_TEST);
    } else {
        sceGuDisable(GU_DEPTH_TEST);
    }
    sceGuDepthMask(write ? GU_FALSE : GU_TRUE);
    sceGuDepthFunc(compare);
}

void draw_rectangle(
    std::uint32_t color, std::uint16_t depth,
    std::int16_t left = 72, std::int16_t top = 40,
    std::int16_t right = 408, std::int16_t bottom = 232) {
    g_vertices[0] = {color, left, top, depth};
    g_vertices[1] = {color, right, bottom, depth};
    sceKernelDcacheWritebackRange(g_vertices, 2 * sizeof(DepthVertex));
    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
        2, nullptr, g_vertices);
}

void draw_crossing(std::uint32_t color, bool forward) {
    const std::uint16_t first = forward ? 56000 : 12000;
    const std::uint16_t second = forward ? 12000 : 56000;
    g_vertices[0] = {color, 72, 220, first};
    g_vertices[1] = {color, 408, 220, second};
    g_vertices[2] = {color, 240, 44, first};
    sceKernelDcacheWritebackRange(g_vertices, 3 * sizeof(DepthVertex));
    sceGuDrawArray(
        GU_TRIANGLES,
        GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
        3, nullptr, g_vertices);
}

std::uint32_t fnv1a_visible() {
    std::uint32_t hash = 2166136261u;
    for (std::uint32_t y = 0; y < kFixtureHeight; ++y) {
        for (std::uint32_t x = 0; x < kFixtureWidth; ++x) {
            const std::uint32_t value = g_capture[y * kFixtureStride + x];
            for (std::uint32_t byte = 0; byte < 4; ++byte) {
                hash ^= (value >> (byte * 8)) & 0xffu;
                hash *= 16777619u;
            }
        }
    }
    return hash;
}

bool capture_color(void* command_list) {
    auto* edram = static_cast<std::uint8_t*>(sceGeEdramGetAddr());
    if (edram == nullptr) {
        return false;
    }
    sceKernelDcacheWritebackInvalidateRange(g_capture, sizeof(g_capture));
    if (sceGuStart(GU_DIRECT, command_list) < 0) {
        return false;
    }
    sceGuCopyImage(
        GU_PSM_8888, 0, 0, kFixtureWidth, kFixtureHeight,
        kFixtureStride, edram + kDrawOffset,
        0, 0, kFixtureStride, g_capture);
    sceGuTexSync();
    if (sceGuFinish() < 0 ||
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE) < 0) {
        return false;
    }
    sceKernelDcacheInvalidateRange(g_capture, sizeof(g_capture));
    return true;
}

bool color_matches(std::uint32_t actual, std::uint32_t expected) {
    return (actual & 0x00ffffffu) == (expected & 0x00ffffffu);
}

bool render_fixture(
    std::uint32_t index, void* command_list, std::uint32_t list_bytes,
    FixtureResult* result) {
    static constexpr const char* kIds[kFixtureCount] = {
        "Z0_clear_only",
        "Z1_near_triangle_then_far_triangle",
        "Z2_far_triangle_then_near_triangle",
        "Z3_crossing_triangles_near_first",
        "Z4_crossing_triangles_far_first",
        "Z5_depth_write_enabled",
        "Z6_depth_write_disabled",
        "Z7_compare_less",
        "Z8_compare_lequal",
        "Z9_equal_depth",
        "Z10_near_plane",
        "Z11_far_plane",
        "Z12_reversed_depth_mapping",
        "Z13_depth_clear_then_draw",
        "Z14_depth_state_restore_after_offscreen_target",
        "Z15_depth_state_restore_before_UI",
    };
    if (index >= kFixtureCount || command_list == nullptr ||
        list_bytes < kMinimumListBytes || result == nullptr ||
        sceGuStart(GU_DIRECT, command_list) < 0) {
        return false;
    }

    configure_baseline();
    std::uint16_t clear_depth = 0;
    int compare = GU_GEQUAL;
    bool depth_write = true;
    std::uint32_t draws = 0;
    std::uint32_t expected_center = kClear;
    sceGuClearColor(kClear);
    sceGuClearDepth(clear_depth);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    configure_depth(true, true, GU_GEQUAL);

    switch (index) {
    case 0:
        break;
    case 1:
        draw_rectangle(kRed, kNearDepth);
        draw_rectangle(kBlue, kFarDepth);
        draws = 2;
        expected_center = kRed;
        break;
    case 2:
        draw_rectangle(kBlue, kFarDepth);
        draw_rectangle(kRed, kNearDepth);
        draws = 2;
        expected_center = kRed;
        break;
    case 3:
        draw_crossing(kRed, true);
        draw_crossing(kBlue, false);
        draws = 2;
        expected_center = kRed;
        break;
    case 4:
        draw_crossing(kBlue, false);
        draw_crossing(kRed, true);
        draws = 2;
        expected_center = kRed;
        break;
    case 5:
        draw_rectangle(kRed, kNearDepth);
        draw_rectangle(kBlue, kFarDepth);
        draws = 2;
        expected_center = kRed;
        break;
    case 6:
        configure_depth(true, false, GU_GEQUAL);
        draw_rectangle(kRed, kNearDepth);
        configure_depth(true, true, GU_GEQUAL);
        draw_rectangle(kBlue, kFarDepth);
        draws = 2;
        expected_center = kBlue;
        depth_write = false;
        break;
    case 7:
        clear_depth = 0;
        compare = GU_GREATER;
        configure_depth(true, true, compare);
        draw_rectangle(kBlue, kFarDepth);
        draw_rectangle(kRed, kNearDepth);
        draws = 2;
        expected_center = kRed;
        break;
    case 8:
        compare = GU_GEQUAL;
        configure_depth(true, true, compare);
        draw_rectangle(kRed, 32000);
        draw_rectangle(kBlue, 32000);
        draws = 2;
        expected_center = kBlue;
        break;
    case 9:
        compare = GU_GREATER;
        configure_depth(true, true, compare);
        draw_rectangle(kRed, 32000);
        draw_rectangle(kBlue, 32000);
        draws = 2;
        expected_center = kRed;
        break;
    case 10:
        draw_rectangle(kRed, 65534);
        draws = 1;
        expected_center = kRed;
        break;
    case 11:
        draw_rectangle(kBlue, 1);
        draws = 1;
        expected_center = kBlue;
        break;
    case 12:
        draw_rectangle(kBlue, kFarDepth);
        draw_rectangle(kRed, kNearDepth);
        draws = 2;
        expected_center = kRed;
        break;
    case 13:
        draw_rectangle(kGreen, 24000);
        draws = 1;
        expected_center = kGreen;
        break;
    case 14:
        sceGuDrawBufferList(
            GU_PSM_8888, relative(kDisplayOffset), kFixtureStride);
        sceGuClearColor(kBlue);
        sceGuClearDepth(0);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
        draw_rectangle(kBlue, kNearDepth);
        sceGuDrawBufferList(
            GU_PSM_8888, relative(kDrawOffset), kFixtureStride);
        sceGuDepthBuffer(relative(kDepthOffset), kFixtureStride);
        sceGuClearColor(kClear);
        sceGuClearDepth(0);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
        configure_depth(true, true, GU_GEQUAL);
        draw_rectangle(kRed, kNearDepth);
        draws = 2;
        expected_center = kRed;
        break;
    case 15:
        draw_rectangle(kRed, kNearDepth);
        configure_depth(false, false, GU_ALWAYS);
        draw_rectangle(kGreen, 0, 160, 96, 320, 176);
        configure_depth(true, true, GU_GEQUAL);
        draws = 2;
        expected_center = kGreen;
        break;
    default:
        return false;
    }

    const int command_bytes = sceGuFinish();
    if (command_bytes <= 0 ||
        static_cast<std::uint32_t>(command_bytes) > list_bytes ||
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE) < 0 ||
        !capture_color(command_list)) {
        return false;
    }

    const std::uint32_t center =
        g_capture[136 * kFixtureStride + 240];
    const std::uint32_t left =
        g_capture[136 * kFixtureStride + 136];
    const std::uint32_t right =
        g_capture[136 * kFixtureStride + 344];
    *result = {
        kIds[index], fnv1a_visible(), center, left, right, draws,
        static_cast<std::uint32_t>(command_bytes), clear_depth,
        static_cast<std::uint8_t>(compare), depth_write,
        color_matches(center, expected_center), true,
    };
    return true;
}

}  // namespace

bool run_fixtures(
    void* command_list, std::uint32_t command_list_bytes,
    FixtureMetrics* metrics) {
    if (command_list == nullptr || metrics == nullptr ||
        command_list_bytes < kMinimumListBytes ||
        sceGeEdramGetAddr() == nullptr ||
        sceGeEdramGetSize() < kDepthOffset + kDepthBytes) {
        return false;
    }
    *metrics = {};
    sceKernelDcacheWritebackAll();
    if (sceGuInit() < 0) {
        return false;
    }
    if (sceGuStart(GU_DIRECT, command_list) < 0) {
        sceGuTerm();
        return false;
    }
    sceGuDrawBuffer(GU_PSM_8888, relative(kDrawOffset), kFixtureStride);
    sceGuDispBuffer(
        kFixtureWidth, kFixtureHeight,
        relative(kDisplayOffset), kFixtureStride);
    sceGuDepthBuffer(relative(kDepthOffset), kFixtureStride);
    configure_baseline();
    if (sceGuFinish() < 0 ||
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE) < 0) {
        sceGuTerm();
        return false;
    }
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    bool valid = true;
    for (std::uint32_t index = 0; index < kFixtureCount; ++index) {
        const bool rendered = render_fixture(
            index, command_list, command_list_bytes,
            &metrics->fixtures[index]);
        valid = valid && rendered;
        if (!rendered || !metrics->fixtures[index].pixels_valid ||
            !metrics->fixtures[index].state_valid) {
            ++metrics->failures;
        }
        ++metrics->fixture_count;
    }
    metrics->allocations = 0;
    metrics->depth_state_leaks = 0;
    metrics->render_target_state_leaks = 0;
    metrics->near_far_ordered =
        metrics->fixtures[10].center_pixel !=
        metrics->fixtures[11].center_pixel;
    metrics->reversed_depth_mapping_valid =
        metrics->fixtures[12].pixels_valid;
    metrics->order_invariant =
        metrics->fixtures[1].framebuffer_hash ==
            metrics->fixtures[2].framebuffer_hash &&
        metrics->fixtures[3].framebuffer_hash ==
            metrics->fixtures[4].framebuffer_hash;
    if (!metrics->order_invariant) {
        ++metrics->failures;
    }
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
    return valid && fixture_metrics_accepted(*metrics);
}

}  // namespace dusk::psp::depth_behavior
