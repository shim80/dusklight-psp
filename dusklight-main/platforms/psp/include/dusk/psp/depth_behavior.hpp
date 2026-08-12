#ifndef DUSK_PSP_DEPTH_BEHAVIOR_HPP
#define DUSK_PSP_DEPTH_BEHAVIOR_HPP

#include <cstdint>

namespace dusk::psp::depth_behavior {

inline constexpr std::uint32_t kFixtureCount = 16;
inline constexpr std::uint32_t kFixtureWidth = 480;
inline constexpr std::uint32_t kFixtureHeight = 272;
inline constexpr std::uint32_t kFixtureStride = 512;

struct FixtureResult {
    const char* id;
    std::uint32_t framebuffer_hash;
    std::uint32_t center_pixel;
    std::uint32_t left_pixel;
    std::uint32_t right_pixel;
    std::uint32_t draw_calls;
    std::uint32_t command_bytes;
    std::uint16_t clear_depth;
    std::uint8_t compare_function;
    bool depth_write;
    bool pixels_valid;
    bool state_valid;
};

struct FixtureMetrics {
    FixtureResult fixtures[kFixtureCount];
    std::uint32_t fixture_count;
    std::uint32_t failures;
    std::uint32_t allocations;
    std::uint32_t depth_state_leaks;
    std::uint32_t render_target_state_leaks;
    bool near_far_ordered;
    bool reversed_depth_mapping_valid;
    bool order_invariant;
};

constexpr bool fixture_metrics_accepted(const FixtureMetrics& metrics) {
    if (metrics.fixture_count != kFixtureCount || metrics.failures != 0 ||
        metrics.allocations != 0 || metrics.depth_state_leaks != 0 ||
        metrics.render_target_state_leaks != 0 ||
        !metrics.near_far_ordered ||
        !metrics.reversed_depth_mapping_valid ||
        !metrics.order_invariant) {
        return false;
    }
    for (std::uint32_t index = 0; index < kFixtureCount; ++index) {
        if (!metrics.fixtures[index].pixels_valid ||
            !metrics.fixtures[index].state_valid ||
            metrics.fixtures[index].command_bytes == 0) {
            return false;
        }
    }
    return true;
}

bool run_fixtures(
    void* command_list,
    std::uint32_t command_list_bytes,
    FixtureMetrics* metrics);

}  // namespace dusk::psp::depth_behavior

#endif
