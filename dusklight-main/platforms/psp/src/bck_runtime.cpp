#include "dusk/psp/bck_runtime.hpp"

#include <cmath>

namespace dusk::psp::animation {
namespace {

constexpr std::uint32_t kClipStride = 48;
constexpr std::uint32_t kTransformStride = 40;

bool finite(float value) {
    return std::isfinite(value);
}

float lerp(float a, float b, float amount) {
    return a + (b - a) * amount;
}

void read_transform(
    const std::uint8_t* bytes, Transform* transform) {
    for (std::uint32_t index = 0; index < 3; ++index) {
        transform->translation[index] =
            playable::read_f32(bytes + index * 4);
        transform->rotation[index] =
            playable::read_f32(bytes + (index + 3) * 4);
        transform->scale[index] =
            playable::read_f32(bytes + (index + 7) * 4);
    }
    transform->rotation[3] = playable::read_f32(bytes + 24);
}

bool normalize_quaternion(float rotation[4]) {
    float length = 0.0f;
    for (std::uint32_t index = 0; index < 4; ++index) {
        length += rotation[index] * rotation[index];
    }
    if (!finite(length) || length <= 0.000001f) {
        return false;
    }
    const float scale = 1.0f / std::sqrt(length);
    for (std::uint32_t index = 0; index < 4; ++index) {
        rotation[index] *= scale;
    }
    return true;
}

}  // namespace

bool PspBckPlayer::resolve_clip(
    const playable::PackageView& package,
    std::uint32_t resource_id, ClipHandle* output) {
    playable::PackageView checked = {};
    if (output == nullptr ||
        playable::validate_dpan(
            package.bytes, package.size, &checked) !=
            playable::PackageError::Ok) {
        ++metrics.invalid_requests;
        return false;
    }
    const std::uint32_t count =
        playable::read_u32(checked.bytes + 16);
    const std::uint32_t table =
        playable::read_u32(checked.bytes + 32);
    const std::uint32_t stride =
        playable::read_u32(checked.bytes + 36);
    if (stride != kClipStride) {
        ++metrics.invalid_requests;
        return false;
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::uint8_t* entry =
            checked.bytes + table + index * stride;
        if (playable::read_u32(entry) == resource_id) {
            *output = {
                checked,
                resource_id,
                playable::read_u32(entry + 8),
                playable::read_u32(entry + 12),
                playable::read_u32(entry + 16),
                playable::read_u32(entry + 24),
                true,
            };
            ++metrics.clip_resolutions;
            return true;
        }
    }
    ++metrics.invalid_requests;
    return false;
}

bool PspBckPlayer::initialize(
    const playable::PackageView& package,
    std::uint32_t resource_id, LoopMode loop_mode,
    float speed, float start_frame, float end_frame) {
    ClipHandle next = {};
    if (!resolve_clip(package, resource_id, &next) ||
        !finite(speed) || !finite(start_frame) ||
        !finite(end_frame)) {
        return false;
    }
    const float clip_end = static_cast<float>(next.frame_max);
    const float selected_end =
        end_frame < 0.0f ? clip_end : end_frame;
    if (start_frame < 0.0f || start_frame >= selected_end ||
        selected_end > clip_end) {
        ++metrics.invalid_requests;
        return false;
    }
    clip_ = next;
    start_ = start_frame;
    end_ = selected_end;
    frame_ = start_frame;
    speed_ = speed;
    loop_mode_ = loop_mode;
    stopped_ = speed == 0.0f;
    return true;
}

bool PspBckPlayer::change_clip(
    const playable::PackageView& package,
    std::uint32_t resource_id) {
    ClipHandle next = {};
    if (!resolve_clip(package, resource_id, &next)) {
        return false;
    }
    clip_ = next;
    start_ = 0.0f;
    end_ = static_cast<float>(next.frame_max);
    frame_ = start_;
    stopped_ = speed_ == 0.0f;
    return true;
}

bool PspBckPlayer::play() {
    if (!clip_.valid || stopped_) {
        return clip_.valid;
    }
    frame_ += speed_;
    ++metrics.plays;
    return normalize_frame();
}

bool PspBckPlayer::normalize_frame() {
    if (frame_ >= start_ && frame_ < end_) {
        return true;
    }
    switch (loop_mode_) {
    case LoopMode::Once:
        frame_ = speed_ < 0.0f ? start_ : end_ - 0.001f;
        speed_ = 0.0f;
        stopped_ = true;
        ++metrics.stops;
        return true;
    case LoopMode::Reset:
        frame_ = start_;
        speed_ = 0.0f;
        stopped_ = true;
        ++metrics.stops;
        return true;
    case LoopMode::Loop:
        while (frame_ >= end_) {
            frame_ -= end_ - start_;
        }
        while (frame_ < start_) {
            frame_ += end_ - start_;
        }
        ++metrics.loops;
        return true;
    case LoopMode::Reverse:
        if (frame_ >= end_) {
            frame_ = end_ - (frame_ - end_);
            speed_ = -speed_;
            ++metrics.reversals;
        }
        if (frame_ < start_) {
            frame_ = start_;
            speed_ = 0.0f;
            stopped_ = true;
            ++metrics.stops;
        }
        return true;
    case LoopMode::LoopReverse:
        if (frame_ >= end_) {
            frame_ = end_ - (frame_ - end_);
            speed_ = -speed_;
            ++metrics.reversals;
        } else if (frame_ < start_) {
            frame_ = start_ + (start_ - frame_);
            speed_ = -speed_;
            ++metrics.reversals;
        }
        ++metrics.loops;
        return true;
    }
    ++metrics.invalid_requests;
    return false;
}

bool PspBckPlayer::set_speed(float speed) {
    if (!finite(speed)) {
        ++metrics.invalid_requests;
        return false;
    }
    speed_ = speed;
    stopped_ = speed == 0.0f;
    return true;
}

bool PspBckPlayer::set_frame(float frame) {
    if (!clip_.valid || !finite(frame) ||
        frame < start_ || frame >= end_) {
        ++metrics.invalid_requests;
        return false;
    }
    frame_ = frame;
    return true;
}

bool PspBckPlayer::set_loop_mode(LoopMode mode) {
    if (static_cast<std::uint8_t>(mode) >
        static_cast<std::uint8_t>(LoopMode::LoopReverse)) {
        ++metrics.invalid_requests;
        return false;
    }
    loop_mode_ = mode;
    return true;
}

bool PspBckPlayer::sample_joint(
    std::uint16_t joint, Transform* output) {
    if (!clip_.valid || output == nullptr ||
        joint >= clip_.joints || clip_.samples < 2) {
        ++metrics.invalid_requests;
        return false;
    }
    const float sample_frame =
        frame_ * static_cast<float>(clip_.samples - 1) /
        static_cast<float>(clip_.frame_max);
    const std::uint32_t first =
        static_cast<std::uint32_t>(std::floor(sample_frame));
    const std::uint32_t second =
        first + 1 < clip_.samples ? first + 1 : first;
    const float amount = sample_frame - static_cast<float>(first);
    const std::uint32_t first_index =
        (first * clip_.joints + joint) * kTransformStride;
    const std::uint32_t second_index =
        (second * clip_.joints + joint) * kTransformStride;
    Transform a = {};
    Transform b = {};
    read_transform(
        clip_.package.bytes + clip_.data_offset + first_index, &a);
    read_transform(
        clip_.package.bytes + clip_.data_offset + second_index, &b);
    for (std::uint32_t index = 0; index < 3; ++index) {
        output->translation[index] = lerp(
            a.translation[index], b.translation[index], amount);
        output->scale[index] =
            lerp(a.scale[index], b.scale[index], amount);
    }
    for (std::uint32_t index = 0; index < 4; ++index) {
        output->rotation[index] =
            lerp(a.rotation[index], b.rotation[index], amount);
    }
    if (!normalize_quaternion(output->rotation)) {
        ++metrics.invalid_requests;
        return false;
    }
    ++metrics.samples;
    return true;
}

bool PspBckPlayer::apply(PoseSink sink, void* user) {
    if (!clip_.valid || sink == nullptr) {
        ++metrics.invalid_requests;
        return false;
    }
    for (std::uint32_t joint = 0; joint < clip_.joints; ++joint) {
        Transform transform = {};
        if (!sample_joint(
                static_cast<std::uint16_t>(joint), &transform) ||
            !sink(user, static_cast<std::uint16_t>(joint), transform)) {
            ++metrics.invalid_requests;
            return false;
        }
    }
    ++metrics.applications;
    return true;
}

float PspBckPlayer::frame() const { return frame_; }
float PspBckPlayer::speed() const { return speed_; }
float PspBckPlayer::start_frame() const { return start_; }
float PspBckPlayer::end_frame() const { return end_; }
LoopMode PspBckPlayer::loop_mode() const { return loop_mode_; }
bool PspBckPlayer::stopped() const { return stopped_; }
const ClipHandle& PspBckPlayer::clip() const { return clip_; }

}  // namespace dusk::psp::animation
