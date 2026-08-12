#include "dusk/psp/frame_profiler.hpp"

#include <limits>

namespace dusk::psp::profiler {
namespace {

enum class Field : std::uint8_t {
    FrameTime,
    CpuTime,
    GeSubmit,
    GeEstimated,
    DrawCalls,
    CommandBytes,
    MemoryUsed,
    EdramUsed,
    Actors,
    Skinning,
    Lighting,
    Shadows,
    Hud,
    Transitions,
};

std::uint32_t value(const FrameSample& sample, Field field) {
    switch (field) {
    case Field::FrameTime: return sample.frame_time_us;
    case Field::CpuTime: return sample.cpu_time_us;
    case Field::GeSubmit: return sample.ge_submit_us;
    case Field::GeEstimated: return sample.ge_time_estimated_us;
    case Field::DrawCalls: return sample.draw_calls;
    case Field::CommandBytes: return sample.command_list_bytes;
    case Field::MemoryUsed: return sample.memory_used_bytes;
    case Field::EdramUsed: return sample.edram_used_bytes;
    case Field::Actors: return sample.costs.actors_us;
    case Field::Skinning: return sample.costs.skinning_us;
    case Field::Lighting: return sample.costs.lighting_us;
    case Field::Shadows: return sample.costs.shadows_us;
    case Field::Hud: return sample.costs.hud_us;
    case Field::Transitions: return sample.costs.transitions_us;
    }
    return 0;
}

void sift_down(std::uint32_t* values, std::uint32_t start,
               std::uint32_t end) {
    std::uint32_t root = start;
    while (root * 2u + 1u <= end) {
        std::uint32_t child = root * 2u + 1u;
        if (child + 1u <= end && values[child] < values[child + 1u]) {
            ++child;
        }
        if (values[root] >= values[child]) {
            return;
        }
        const std::uint32_t temporary = values[root];
        values[root] = values[child];
        values[child] = temporary;
        root = child;
    }
}

void heap_sort(std::uint32_t* values, std::uint32_t count) {
    if (count < 2) {
        return;
    }
    for (std::uint32_t start = count / 2u; start > 0; --start) {
        sift_down(values, start - 1u, count - 1u);
    }
    for (std::uint32_t end = count - 1u; end > 0; --end) {
        const std::uint32_t temporary = values[end];
        values[end] = values[0];
        values[0] = temporary;
        sift_down(values, 0, end - 1u);
    }
}

std::uint32_t percentile_index(std::uint32_t count,
                               std::uint32_t per_mille) {
    const std::uint64_t rank =
        (static_cast<std::uint64_t>(count) * per_mille + 999u) / 1000u;
    return static_cast<std::uint32_t>(rank == 0 ? 0 : rank - 1u);
}

Distribution distribution(const FrameSample* samples, std::uint32_t count,
                          Field field, std::uint32_t* scratch,
                          std::uint32_t scratch_count) {
    Distribution result = {};
    if (samples == nullptr || count == 0 || scratch == nullptr ||
        scratch_count < count) {
        return result;
    }
    std::uint64_t sum = 0;
    for (std::uint32_t index = 0; index < count; ++index) {
        scratch[index] = value(samples[index], field);
        sum += scratch[index];
    }
    heap_sort(scratch, count);
    result.minimum = scratch[0];
    result.mean = sum / count;
    result.median = count % 2u == 0
        ? static_cast<std::uint32_t>(
              (static_cast<std::uint64_t>(scratch[count / 2u - 1u]) +
               scratch[count / 2u]) / 2u)
        : scratch[count / 2u];
    result.percentile_95 = scratch[percentile_index(count, 950)];
    result.percentile_99 = scratch[percentile_index(count, 990)];
    result.maximum = scratch[count - 1u];
    return result;
}

std::uint64_t elapsed(const FrameSample* samples, std::uint32_t begin,
                      std::uint32_t end) {
    std::uint64_t result = 0;
    for (std::uint32_t index = begin; index < end; ++index) {
        result += samples[index].frame_time_us;
    }
    return result;
}

std::uint32_t low_fps_milli(std::uint32_t* sorted_frame_times,
                            std::uint32_t count,
                            std::uint32_t fraction_per_mille) {
    const std::uint32_t selected = static_cast<std::uint32_t>(
        (static_cast<std::uint64_t>(count) * fraction_per_mille + 999u) /
        1000u);
    const std::uint32_t low_count = selected == 0 ? 1u : selected;
    std::uint64_t slow_elapsed = 0;
    for (std::uint32_t index = count - low_count; index < count; ++index) {
        slow_elapsed += sorted_frame_times[index];
    }
    return fps_milli(low_count, slow_elapsed);
}

}  // namespace

bool FrameProfiler::begin(const Contract& contract_value) {
    if (contract_value.measured_frames == 0 ||
        contract_value.measured_frames > kMaximumFrames) {
        return false;
    }
    contract_ = contract_value;
    count_ = 0;
    started_ = true;
    return true;
}

bool FrameProfiler::record(const FrameSample& sample) {
    if (!started_ || count_ >= contract_.measured_frames ||
        sample.frame_time_us == 0) {
        return false;
    }
    samples_[count_++] = sample;
    return true;
}

FrameDerived FrameProfiler::derived(std::uint32_t sample_index) const {
    if (sample_index >= count_) {
        return {};
    }
    const std::uint32_t begin_60 =
        sample_index + 1u > 60u ? sample_index + 1u - 60u : 0u;
    const std::uint32_t begin_300 =
        sample_index + 1u > 300u ? sample_index + 1u - 300u : 0u;
    return {
        fps_milli(1, samples_[sample_index].frame_time_us),
        fps_milli(sample_index + 1u - begin_60,
                  elapsed(samples_, begin_60, sample_index + 1u)),
        fps_milli(sample_index + 1u - begin_300,
                  elapsed(samples_, begin_300, sample_index + 1u)),
    };
}

Summary FrameProfiler::summarize(std::uint32_t* scratch,
                                 std::uint32_t scratch_count) const {
    Summary result = {};
    if (count_ == 0 || scratch == nullptr || scratch_count < count_) {
        return result;
    }
    result.sample_count = count_;
    result.frame_time_us =
        distribution(samples_, count_, Field::FrameTime, scratch, scratch_count);
    result.cpu_time_us =
        distribution(samples_, count_, Field::CpuTime, scratch, scratch_count);
    result.ge_submit_us =
        distribution(samples_, count_, Field::GeSubmit, scratch, scratch_count);
    result.ge_time_estimated_us = distribution(
        samples_, count_, Field::GeEstimated, scratch, scratch_count);
    result.draw_calls =
        distribution(samples_, count_, Field::DrawCalls, scratch, scratch_count);
    result.command_list_bytes = distribution(
        samples_, count_, Field::CommandBytes, scratch, scratch_count);
    result.memory_used_bytes = distribution(
        samples_, count_, Field::MemoryUsed, scratch, scratch_count);
    result.edram_used_bytes = distribution(
        samples_, count_, Field::EdramUsed, scratch, scratch_count);
    result.actors_us =
        distribution(samples_, count_, Field::Actors, scratch, scratch_count);
    result.skinning_us =
        distribution(samples_, count_, Field::Skinning, scratch, scratch_count);
    result.lighting_us =
        distribution(samples_, count_, Field::Lighting, scratch, scratch_count);
    result.shadows_us =
        distribution(samples_, count_, Field::Shadows, scratch, scratch_count);
    result.hud_us =
        distribution(samples_, count_, Field::Hud, scratch, scratch_count);
    result.transitions_us = distribution(
        samples_, count_, Field::Transitions, scratch, scratch_count);
    result.fps_average_milli = fps_milli(count_, elapsed(samples_, 0, count_));

    for (std::uint32_t index = 0; index < count_; ++index) {
        scratch[index] = samples_[index].frame_time_us;
        result.total_allocations += samples_[index].allocations;
    }
    heap_sort(scratch, count_);
    result.fps_1_percent_low_milli =
        low_fps_milli(scratch, count_, 10);
    result.fps_0_1_percent_low_milli =
        low_fps_milli(scratch, count_, 1);
    result.frame_time_worst_us = result.frame_time_us.maximum;
    result.peak_memory_used_bytes = result.memory_used_bytes.maximum;
    result.peak_edram_used_bytes = result.edram_used_bytes.maximum;
    result.peak_draw_calls = result.draw_calls.maximum;
    result.peak_command_list_bytes = result.command_list_bytes.maximum;
    return result;
}

const Contract& FrameProfiler::contract() const {
    return contract_;
}

const FrameSample* FrameProfiler::samples() const {
    return samples_;
}

std::uint32_t FrameProfiler::count() const {
    return count_;
}

bool FrameProfiler::full() const {
    return started_ && count_ == contract_.measured_frames;
}

const char* profile_name(Profile profile) {
    switch (profile) {
    case Profile::Functional: return "functional";
    case Profile::Performance: return "performance";
    case Profile::PspConservative: return "psp_conservative";
    }
    return "invalid";
}

const char* scene_name(Scene scene) {
    switch (scene) {
    case Scene::BootIntro: return "boot_intro";
    case Scene::Title: return "title_screen";
    case Scene::LinkIdle: return "link_idle";
    case Scene::RoomStress: return "room_stress";
    case Scene::Transition: return "transition";
    }
    return "invalid";
}

const char* mode_name(Scene scene) {
    switch (scene) {
    case Scene::BootIntro: return "benchmark_boot_intro";
    case Scene::Title: return "benchmark_title";
    case Scene::LinkIdle: return "benchmark_link_idle";
    case Scene::RoomStress: return "benchmark_room_stress";
    case Scene::Transition: return "benchmark_transition";
    }
    return "invalid";
}

std::uint32_t fps_milli(std::uint64_t frames,
                        std::uint64_t elapsed_microseconds) {
    if (frames == 0 || elapsed_microseconds == 0 ||
        frames > std::numeric_limits<std::uint64_t>::max() / 1000000000u) {
        return 0;
    }
    const std::uint64_t result =
        (frames * 1000000000u + elapsed_microseconds / 2u) /
        elapsed_microseconds;
    return result > std::numeric_limits<std::uint32_t>::max()
        ? std::numeric_limits<std::uint32_t>::max()
        : static_cast<std::uint32_t>(result);
}

}  // namespace dusk::psp::profiler
