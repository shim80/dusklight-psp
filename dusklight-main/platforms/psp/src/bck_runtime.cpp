#include "dusk/psp/bck_runtime.hpp"

#include <cmath>

namespace dusk::psp::animation {
namespace {

constexpr std::uint32_t kClipStride = 48;
constexpr std::uint32_t kTransformStride = 40;
constexpr std::uint32_t kBrkClipStride = 32;
constexpr std::uint32_t kBrkChannelStride = 8;
constexpr std::uint32_t kBrkValueStride = 16;

bool finite(float value) {
    return std::isfinite(value);
}

bool range(
    std::uint32_t offset, std::uint32_t count,
    std::uint32_t stride, std::uint32_t size) {
    return offset <= size && stride != 0 &&
           count <= (size - offset) / stride;
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

bool normalize_player_frame(
    float* frame, float* speed, float start, float end,
    LoopMode mode, bool* stopped, Metrics* metrics) {
    if (*frame >= start && *frame < end) {
        return true;
    }
    switch (mode) {
    case LoopMode::Once:
        *frame = *speed < 0.0f ? start : end - 0.001f;
        *speed = 0.0f;
        *stopped = true;
        ++metrics->stops;
        return true;
    case LoopMode::Reset:
        *frame = start;
        *speed = 0.0f;
        *stopped = true;
        ++metrics->stops;
        return true;
    case LoopMode::Loop:
        while (*frame >= end) {
            *frame -= end - start;
        }
        while (*frame < start) {
            *frame += end - start;
        }
        ++metrics->loops;
        return true;
    case LoopMode::Reverse:
        if (*frame >= end) {
            *frame = end - (*frame - end);
            *speed = -*speed;
            ++metrics->reversals;
        }
        if (*frame < start) {
            *frame = start;
            *speed = 0.0f;
            *stopped = true;
            ++metrics->stops;
        }
        return true;
    case LoopMode::LoopReverse:
        if (*frame >= end) {
            *frame = end - (*frame - end);
            *speed = -*speed;
            ++metrics->reversals;
        } else if (*frame < start) {
            *frame = start + (start - *frame);
            *speed = -*speed;
            ++metrics->reversals;
        }
        ++metrics->loops;
        return true;
    }
    ++metrics->invalid_requests;
    return false;
}

bool valid_loop_mode(LoopMode mode) {
    return static_cast<std::uint8_t>(mode) <=
           static_cast<std::uint8_t>(LoopMode::LoopReverse);
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
        !finite(end_frame) || !valid_loop_mode(loop_mode)) {
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
    return normalize_player_frame(
        &frame_, &speed_, start_, end_, loop_mode_,
        &stopped_, &metrics);
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
    if (!valid_loop_mode(mode)) {
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

bool PspBrkPlayer::resolve_clip(
    const playable::PackageView& package,
    std::uint32_t resource_id, BrkClipHandle* output) {
    playable::PackageView checked = {};
    if (output == nullptr ||
        playable::validate_package(
            package.bytes, package.size, "DPBR", &checked) !=
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
    if (count == 0 || stride != kBrkClipStride ||
        !range(table, count, stride, checked.size)) {
        ++metrics.invalid_requests;
        return false;
    }
    for (std::uint32_t clip_index = 0;
         clip_index < count; ++clip_index) {
        const std::uint8_t* entry =
            checked.bytes + table + clip_index * stride;
        if (playable::read_u32(entry) != resource_id) {
            continue;
        }
        const std::uint32_t frame_max =
            playable::read_u32(entry + 8);
        const std::uint32_t samples =
            playable::read_u32(entry + 12);
        const std::uint32_t channels =
            playable::read_u32(entry + 16);
        const std::uint32_t channel_offset =
            playable::read_u32(entry + 20);
        const std::uint32_t data_offset =
            playable::read_u32(entry + 24);
        const std::uint32_t data_size =
            playable::read_u32(entry + 28);
        if (frame_max == 0 || samples < 2 ||
            channels == 0 || channels > 32 ||
            !range(
                channel_offset, channels,
                kBrkChannelStride, checked.size) ||
            data_offset > checked.size ||
            data_size > checked.size - data_offset) {
            ++metrics.invalid_requests;
            return false;
        }
        for (std::uint32_t channel = 0;
             channel < channels; ++channel) {
            const std::uint8_t* descriptor =
                checked.bytes + channel_offset +
                channel * kBrkChannelStride;
            const std::uint8_t kind = descriptor[2];
            const std::uint8_t register_index = descriptor[3];
            const std::uint32_t sample_offset =
                playable::read_u32(descriptor + 4);
            if (kind > static_cast<std::uint8_t>(
                           TevRegisterKind::Konst) ||
                register_index >= 4 ||
                sample_offset > data_size ||
                samples >
                    (data_size - sample_offset) / kBrkValueStride) {
                ++metrics.invalid_requests;
                return false;
            }
            for (std::uint32_t sample = 0;
                 sample < samples; ++sample) {
                const std::uint8_t* value =
                    checked.bytes + data_offset + sample_offset +
                    sample * kBrkValueStride;
                for (std::uint32_t component = 0;
                     component < 4; ++component) {
                    if (!finite(playable::read_f32(
                            value + component * 4))) {
                        ++metrics.invalid_requests;
                        return false;
                    }
                }
            }
        }
        *output = {
            checked,
            resource_id,
            frame_max,
            samples,
            channels,
            channel_offset,
            data_offset,
            data_size,
            true,
        };
        ++metrics.clip_resolutions;
        return true;
    }
    ++metrics.invalid_requests;
    return false;
}

bool PspBrkPlayer::initialize(
    const playable::PackageView& package,
    std::uint32_t resource_id, LoopMode loop_mode,
    float speed, float start_frame, float end_frame) {
    BrkClipHandle next = {};
    if (!resolve_clip(package, resource_id, &next) ||
        !finite(speed) || !finite(start_frame) ||
        !finite(end_frame) || !valid_loop_mode(loop_mode)) {
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

bool PspBrkPlayer::change_clip(
    const playable::PackageView& package,
    std::uint32_t resource_id) {
    BrkClipHandle next = {};
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

bool PspBrkPlayer::play() {
    if (!clip_.valid || stopped_) {
        return clip_.valid;
    }
    frame_ += speed_;
    ++metrics.plays;
    return normalize_frame();
}

bool PspBrkPlayer::normalize_frame() {
    return normalize_player_frame(
        &frame_, &speed_, start_, end_, loop_mode_,
        &stopped_, &metrics);
}

bool PspBrkPlayer::set_speed(float speed) {
    if (!finite(speed)) {
        ++metrics.invalid_requests;
        return false;
    }
    speed_ = speed;
    stopped_ = speed == 0.0f;
    return true;
}

bool PspBrkPlayer::set_frame(float frame) {
    if (!clip_.valid || !finite(frame) ||
        frame < start_ || frame >= end_) {
        ++metrics.invalid_requests;
        return false;
    }
    frame_ = frame;
    return true;
}

bool PspBrkPlayer::set_loop_mode(LoopMode mode) {
    if (!valid_loop_mode(mode)) {
        ++metrics.invalid_requests;
        return false;
    }
    loop_mode_ = mode;
    return true;
}

bool PspBrkPlayer::sample_channel(
    std::uint16_t channel, TevRegisterValue* output) {
    if (!clip_.valid || output == nullptr ||
        channel >= clip_.channels || clip_.samples < 2) {
        ++metrics.invalid_requests;
        return false;
    }
    const std::uint8_t* descriptor =
        clip_.package.bytes + clip_.channel_offset +
        channel * kBrkChannelStride;
    const std::uint32_t sample_offset =
        playable::read_u32(descriptor + 4);
    const float sample_frame =
        frame_ * static_cast<float>(clip_.samples - 1) /
        static_cast<float>(clip_.frame_max);
    const std::uint32_t first =
        static_cast<std::uint32_t>(std::floor(sample_frame));
    const std::uint32_t second =
        first + 1 < clip_.samples ? first + 1 : first;
    const float amount = sample_frame - static_cast<float>(first);
    const std::uint8_t* first_value =
        clip_.package.bytes + clip_.data_offset + sample_offset +
        first * kBrkValueStride;
    const std::uint8_t* second_value =
        clip_.package.bytes + clip_.data_offset + sample_offset +
        second * kBrkValueStride;
    output->material = playable::read_u16(descriptor);
    output->kind = static_cast<TevRegisterKind>(descriptor[2]);
    output->index = descriptor[3];
    for (std::uint32_t component = 0;
         component < 4; ++component) {
        output->rgba[component] = lerp(
            playable::read_f32(first_value + component * 4),
            playable::read_f32(second_value + component * 4),
            amount);
    }
    ++metrics.samples;
    return true;
}

bool PspBrkPlayer::apply(TevRegisterSink sink, void* user) {
    if (!clip_.valid || sink == nullptr) {
        ++metrics.invalid_requests;
        return false;
    }
    for (std::uint32_t channel = 0;
         channel < clip_.channels; ++channel) {
        TevRegisterValue value = {};
        if (!sample_channel(
                static_cast<std::uint16_t>(channel), &value) ||
            !sink(user, value)) {
            ++metrics.invalid_requests;
            return false;
        }
    }
    ++metrics.applications;
    return true;
}

float PspBrkPlayer::frame() const { return frame_; }
float PspBrkPlayer::speed() const { return speed_; }
float PspBrkPlayer::start_frame() const { return start_; }
float PspBrkPlayer::end_frame() const { return end_; }
LoopMode PspBrkPlayer::loop_mode() const { return loop_mode_; }
bool PspBrkPlayer::stopped() const { return stopped_; }
const BrkClipHandle& PspBrkPlayer::clip() const { return clip_; }

}  // namespace dusk::psp::animation
