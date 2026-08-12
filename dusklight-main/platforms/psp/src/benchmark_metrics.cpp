#include "dusk/psp/benchmark_metrics.hpp"

#include <cstring>

namespace dusk::psp::benchmark {
namespace {

bool line_present(
    const char* begin,
    const char* end,
    const char* key) {
    const std::size_t key_length = std::strlen(key);
    for (const char* cursor = begin;
         cursor + key_length + 1 <= end;
         ++cursor) {
        if ((cursor == begin || cursor[-1] == '\n') &&
            std::memcmp(cursor, key, key_length) == 0 &&
            cursor[key_length] == '=') {
            return true;
        }
    }
    return false;
}

const char* find_line(
    const char* begin,
    const char* end,
    const char* line) {
    const std::size_t length = std::strlen(line);
    for (const char* cursor = begin;
         cursor + length <= end;
         ++cursor) {
        if ((cursor == begin || cursor[-1] == '\n') &&
            std::memcmp(cursor, line, length) == 0 &&
            (cursor + length == end || cursor[length] == '\n')) {
            return cursor;
        }
    }
    return nullptr;
}

bool scenario_complete(
    const char* text,
    const char* end,
    const char* name) {
    char begin_line[64] = {};
    char end_line[64] = {};
    std::strcpy(begin_line, "scenario_begin=");
    std::strcat(begin_line, name);
    std::strcpy(end_line, "scenario_end=");
    std::strcat(end_line, name);
    const char* begin = find_line(text, end, begin_line);
    const char* finish =
        begin != nullptr ? find_line(begin, end, end_line) : nullptr;
    if (begin == nullptr || finish == nullptr || finish <= begin) {
        return false;
    }
    static constexpr const char* required[] = {
        "scenario_name",
        "culling_enabled",
        "sample_count",
        "cpu_update_us_min",
        "cpu_update_us_mean",
        "cpu_update_us_median",
        "cpu_update_us_p90",
        "cpu_update_us_p95",
        "cpu_update_us_p99",
        "cpu_update_us_max",
        "adapter_us_min",
        "adapter_us_mean",
        "adapter_us_median",
        "adapter_us_p90",
        "adapter_us_p95",
        "adapter_us_p99",
        "adapter_us_max",
        "gu_submission_us_min",
        "gu_submission_us_mean",
        "gu_submission_us_median",
        "gu_submission_us_p90",
        "gu_submission_us_p95",
        "gu_submission_us_p99",
        "gu_submission_us_max",
        "ge_sync_us_min",
        "ge_sync_us_mean",
        "ge_sync_us_median",
        "ge_sync_us_p90",
        "ge_sync_us_p95",
        "ge_sync_us_p99",
        "ge_sync_us_max",
        "presentation_us_min",
        "presentation_us_mean",
        "presentation_us_median",
        "presentation_us_p90",
        "presentation_us_p95",
        "presentation_us_p99",
        "presentation_us_max",
        "frame_total_us_min",
        "frame_total_us_mean",
        "frame_total_us_median",
        "frame_total_us_p90",
        "frame_total_us_p95",
        "frame_total_us_p99",
        "frame_total_us_max",
        "frames_over_16667_us",
        "frames_over_33333_us",
        "frames_over_50000_us",
        "frames_over_66667_us",
        "command_list_bytes_min",
        "command_list_bytes_mean",
        "command_list_bytes_max",
        "cache_writeback_operations_average",
        "cache_invalidate_operations_average",
    };
    for (const char* key : required) {
        if (!line_present(begin, finish, key)) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool metrics_schema_complete(
    const char* text, std::uint32_t size) {
    if (text == nullptr || size == 0) {
        return false;
    }
    const char* end = text + size;
    static constexpr const char* required[] = {
        "benchmark_version",
        "target_declared",
        "run_label",
        "build_commit",
        "eboot_sha256",
        "dpmd_sha256_expected",
        "pose_contract",
        "mode",
        "triangle_count",
        "vertex_count",
        "index_count",
        "vertex_bytes",
        "index_bytes",
        "chunk_count",
        "draw_call_count",
        "package_bytes",
        "package_crc_expected",
        "package_crc_actual",
        "warmup_frames",
        "measured_frames",
        "cpu_clock_mhz",
        "bus_clock_mhz",
        "pll_clock_mhz",
        "framebuffer_format",
        "depth_format",
        "visible_width",
        "visible_height",
        "stride",
        "command_list_capacity",
        "vblank_wait_enabled",
        "auto_rotation_speed",
        "allocation_count_during_frame",
        "free_memory_before_load",
        "free_memory_after_load",
        "free_memory_before_benchmark",
        "free_memory_min_during_benchmark",
        "free_memory_after_benchmark",
        "free_memory_after_release",
        "guard_regions_valid",
        "pixel_checks_before_valid",
        "pixel_checks_after_valid",
        "synchronization",
        "hardware_validation",
        "diagnostic_only",
        "error_code",
    };
    for (const char* key : required) {
        if (!line_present(text, end, key)) {
            return false;
        }
    }
    return scenario_complete(text, end, "culling_off") &&
           scenario_complete(text, end, "culling_on");
}

}  // namespace dusk::psp::benchmark
