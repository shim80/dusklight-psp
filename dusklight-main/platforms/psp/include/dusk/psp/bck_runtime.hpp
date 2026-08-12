#ifndef DUSK_PSP_BCK_RUNTIME_HPP
#define DUSK_PSP_BCK_RUNTIME_HPP

#include "dusk/psp/playable_package.hpp"

#include <cstdint>

namespace dusk::psp::animation {

enum class LoopMode : std::uint8_t {
    Once = 0,
    Reset = 1,
    Loop = 2,
    Reverse = 3,
    LoopReverse = 4,
};

struct Transform {
    float translation[3];
    float rotation[4];
    float scale[3];
};

struct ClipHandle {
    playable::PackageView package;
    std::uint32_t resource_id;
    std::uint32_t frame_max;
    std::uint32_t samples;
    std::uint32_t joints;
    std::uint32_t data_offset;
    bool valid;
};

struct Metrics {
    std::uint32_t clip_resolutions;
    std::uint32_t plays;
    std::uint32_t loops;
    std::uint32_t stops;
    std::uint32_t reversals;
    std::uint32_t samples;
    std::uint32_t applications;
    std::uint32_t invalid_requests;
};

using PoseSink = bool (*)(
    void* user, std::uint16_t joint,
    const Transform& transform);

class PspBckPlayer {
public:
    bool initialize(
        const playable::PackageView& package,
        std::uint32_t resource_id, LoopMode loop_mode,
        float speed, float start_frame, float end_frame);
    bool change_clip(
        const playable::PackageView& package,
        std::uint32_t resource_id);
    bool play();
    bool set_speed(float speed);
    bool set_frame(float frame);
    bool set_loop_mode(LoopMode mode);
    bool sample_joint(std::uint16_t joint, Transform* output);
    bool apply(PoseSink sink, void* user);

    float frame() const;
    float speed() const;
    float start_frame() const;
    float end_frame() const;
    LoopMode loop_mode() const;
    bool stopped() const;
    const ClipHandle& clip() const;

    Metrics metrics = {};

private:
    bool resolve_clip(
        const playable::PackageView& package,
        std::uint32_t resource_id, ClipHandle* output);
    bool normalize_frame();

    ClipHandle clip_ = {};
    float frame_ = 0.0f;
    float speed_ = 0.0f;
    float start_ = 0.0f;
    float end_ = 0.0f;
    LoopMode loop_mode_ = LoopMode::Once;
    bool stopped_ = true;
};

bool source_compatibility_surface_valid();

}  // namespace dusk::psp::animation

#endif
