#ifndef DUSK_PSP_FRAME_PROFILER_HPP
#define DUSK_PSP_FRAME_PROFILER_HPP

#include <cstdint>

namespace dusk::psp::profiler {

inline constexpr std::uint32_t kMaximumFrames = 1800;
inline constexpr std::uint32_t kFpsScale = 1000;

enum class Profile : std::uint8_t {
    Functional,
    Performance,
    PspConservative,
};

enum class Scene : std::uint8_t {
    BootIntro,
    Title,
    LinkIdle,
    RoomStress,
    Transition,
};

struct FrameCosts {
    std::uint32_t actors_us;
    std::uint32_t skinning_us;
    std::uint32_t lighting_us;
    std::uint32_t shadows_us;
    std::uint32_t hud_us;
    std::uint32_t transitions_us;
};

struct FrameSample {
    std::uint32_t frame_number;
    std::uint32_t frame_time_us;
    std::uint32_t cpu_time_us;
    std::uint32_t ge_submit_us;
    std::uint32_t ge_time_estimated_us;
    std::uint32_t free_memory_bytes;
    std::uint32_t memory_used_bytes;
    std::uint32_t edram_used_bytes;
    std::uint32_t edram_free_bytes;
    std::uint32_t draw_calls;
    std::uint32_t command_list_bytes;
    std::uint32_t vertices;
    std::uint32_t triangles;
    std::uint32_t actor_count;
    std::uint32_t allocations;
    FrameCosts costs;
};

struct FrameDerived {
    std::uint32_t fps_instant_milli;
    std::uint32_t fps_average_60_milli;
    std::uint32_t fps_average_300_milli;
};

struct Distribution {
    std::uint32_t minimum;
    std::uint64_t mean;
    std::uint32_t median;
    std::uint32_t percentile_95;
    std::uint32_t percentile_99;
    std::uint32_t maximum;
};

struct Summary {
    std::uint32_t sample_count;
    Distribution frame_time_us;
    Distribution cpu_time_us;
    Distribution ge_submit_us;
    Distribution ge_time_estimated_us;
    Distribution draw_calls;
    Distribution command_list_bytes;
    Distribution memory_used_bytes;
    Distribution edram_used_bytes;
    Distribution actors_us;
    Distribution skinning_us;
    Distribution lighting_us;
    Distribution shadows_us;
    Distribution hud_us;
    Distribution transitions_us;
    std::uint32_t fps_average_milli;
    std::uint32_t fps_1_percent_low_milli;
    std::uint32_t fps_0_1_percent_low_milli;
    std::uint32_t frame_time_worst_us;
    std::uint32_t peak_memory_used_bytes;
    std::uint32_t peak_edram_used_bytes;
    std::uint32_t peak_draw_calls;
    std::uint32_t peak_command_list_bytes;
    std::uint32_t total_allocations;
};

struct Contract {
    Profile profile;
    Scene scene;
    std::uint32_t warmup_frames;
    std::uint32_t measured_frames;
    bool framebuffer_readback;
    bool captures;
    bool debug;
    bool hardware_renderer_required;
    bool native_psp_resolution;
};

class FrameProfiler {
public:
    bool begin(const Contract& contract);
    bool record(const FrameSample& sample);
    FrameDerived derived(std::uint32_t sample_index) const;
    Summary summarize(std::uint32_t* scratch,
                      std::uint32_t scratch_count) const;

    const Contract& contract() const;
    const FrameSample* samples() const;
    std::uint32_t count() const;
    bool full() const;

private:
    Contract contract_ = {};
    FrameSample samples_[kMaximumFrames] = {};
    std::uint32_t count_ = 0;
    bool started_ = false;
};

constexpr Contract contract(Profile profile, Scene scene) {
    const bool functional = profile == Profile::Functional;
    return {
        profile,
        scene,
        120,
        scene == Scene::LinkIdle ? 1800u : 600u,
        functional,
        functional,
        functional,
        profile != Profile::Functional,
        true,
    };
}

const char* profile_name(Profile profile);
const char* scene_name(Scene scene);
const char* mode_name(Scene scene);
std::uint32_t fps_milli(std::uint64_t frames,
                        std::uint64_t elapsed_microseconds);

}  // namespace dusk::psp::profiler

#endif
