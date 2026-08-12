#ifndef DUSK_PSP_BENCHMARK_STATS_HPP
#define DUSK_PSP_BENCHMARK_STATS_HPP

#include <cstdint>

namespace dusk::psp::benchmark {

inline constexpr std::uint32_t kWarmupFrames = 120;
inline constexpr std::uint32_t kMeasuredFrames = 600;

struct FrameSample {
    std::uint32_t cpu_update_us;
    std::uint32_t adapter_us;
    std::uint32_t gu_submission_us;
    std::uint32_t ge_sync_us;
    std::uint32_t presentation_us;
    std::uint32_t frame_total_us;
    std::uint32_t command_list_bytes;
    std::uint32_t cache_writeback_operations;
    std::uint32_t cache_invalidate_operations;
};

struct SampleBuffer {
    FrameSample values[kMeasuredFrames];
    std::uint32_t count;
};

struct Distribution {
    std::uint32_t minimum;
    std::uint64_t mean;
    std::uint32_t median;
    std::uint32_t percentile_90;
    std::uint32_t percentile_95;
    std::uint32_t percentile_99;
    std::uint32_t maximum;
    std::uint64_t mean_absolute_deviation;
    std::uint32_t sample_count;
};

struct ScenarioStatistics {
    Distribution cpu_update_us;
    Distribution adapter_us;
    Distribution gu_submission_us;
    Distribution ge_sync_us;
    Distribution presentation_us;
    Distribution frame_total_us;
    Distribution command_list_bytes;
    Distribution cache_writeback_operations;
    Distribution cache_invalidate_operations;
    std::uint32_t frames_over_16667_us;
    std::uint32_t frames_over_33333_us;
    std::uint32_t frames_over_50000_us;
    std::uint32_t frames_over_66667_us;
};

enum class Field : std::uint32_t {
    CpuUpdate,
    Adapter,
    GuSubmission,
    GeSync,
    Presentation,
    FrameTotal,
    CommandListBytes,
    CacheWritebackOperations,
    CacheInvalidateOperations,
};

bool collect(SampleBuffer* buffer, const FrameSample& sample);
std::uint32_t ticks_to_microseconds(
    std::uint64_t ticks, std::uint64_t ticks_per_second);
Distribution summarize(
    const SampleBuffer& buffer,
    Field field,
    std::uint32_t* scratch,
    std::uint32_t scratch_count);
ScenarioStatistics calculate(
    const SampleBuffer& buffer,
    std::uint32_t* scratch,
    std::uint32_t scratch_count);

}  // namespace dusk::psp::benchmark

#endif
