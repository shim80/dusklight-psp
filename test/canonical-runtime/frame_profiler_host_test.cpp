#include "dusk/psp/frame_profiler.hpp"

#include <array>
#include <cstdint>
#include <cstdio>

namespace profiler = dusk::psp::profiler;

namespace {

profiler::FrameSample sample(std::uint32_t frame,
                             std::uint32_t frame_time) {
    return {
        frame, frame_time, 4000, 500, 600, 20u * 1024u * 1024u,
        4u * 1024u * 1024u, 2u * 1024u * 1024u,
        2u * 1024u * 1024u, 42, 32768, 3000, 1000, 8, 0,
        {100, 200, 300, 400, 500, 600},
    };
}

bool profiles_are_strictly_separated() {
    const profiler::Contract functional = profiler::contract(
        profiler::Profile::Functional, profiler::Scene::Title);
    const profiler::Contract performance = profiler::contract(
        profiler::Profile::Performance, profiler::Scene::Title);
    const profiler::Contract conservative = profiler::contract(
        profiler::Profile::PspConservative, profiler::Scene::Title);
    return functional.framebuffer_readback && functional.captures &&
           functional.debug && !functional.hardware_renderer_required &&
           !performance.framebuffer_readback && !performance.captures &&
           !performance.debug && performance.hardware_renderer_required &&
           !conservative.framebuffer_readback && !conservative.captures &&
           conservative.hardware_renderer_required &&
           functional.native_psp_resolution &&
           performance.native_psp_resolution &&
           conservative.native_psp_resolution;
}

}  // namespace

int main() {
    static profiler::FrameProfiler frame_profiler;
    const profiler::Contract idle = profiler::contract(
        profiler::Profile::Performance, profiler::Scene::LinkIdle);
    if (!frame_profiler.begin(idle) || idle.measured_frames != 1800 ||
        !profiles_are_strictly_separated()) {
        return 2;
    }
    for (std::uint32_t frame = 0; frame < idle.measured_frames; ++frame) {
        const std::uint32_t frame_time =
            frame == idle.measured_frames - 1 ? 33334u : 16667u;
        if (!frame_profiler.record(sample(frame, frame_time))) {
            return 3;
        }
    }
    static std::array<std::uint32_t, profiler::kMaximumFrames> scratch = {};
    const profiler::Summary summary =
        frame_profiler.summarize(scratch.data(), scratch.size());
    const profiler::FrameDerived last =
        frame_profiler.derived(frame_profiler.count() - 1u);
    const bool valid =
        frame_profiler.full() &&
        summary.sample_count == 1800 &&
        summary.frame_time_us.minimum == 16667 &&
        summary.frame_time_us.maximum == 33334 &&
        summary.frame_time_us.percentile_95 == 16667 &&
        summary.frame_time_us.percentile_99 == 16667 &&
        summary.fps_average_milli > 59900 &&
        summary.fps_average_milli < 60000 &&
        summary.fps_1_percent_low_milli == 56841 &&
        summary.fps_0_1_percent_low_milli == 39999 &&
        summary.peak_draw_calls == 42 &&
        summary.peak_command_list_bytes == 32768 &&
        summary.total_allocations == 0 &&
        last.fps_instant_milli == 29999 &&
        last.fps_average_60_milli > 59000 &&
        last.fps_average_300_milli > 59700 &&
        profiler::fps_milli(1, 0) == 0 &&
        profiler::fps_milli(1, 16667) == 59999;
    if (!valid) {
        return 4;
    }
    std::printf(
        "FRAME_PROFILER_HOST_OK samples=1800 rolling=60,300 "
        "percentiles=p50,p95,p99 lows=1,0.1 allocation_free=true\n");
    return 0;
}
