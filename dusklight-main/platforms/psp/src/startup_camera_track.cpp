#include "dusk/psp/startup_camera_track.hpp"

#include <cmath>
#include <cstring>

namespace dusk::psp::camera {
namespace {

std::uint16_t read_u16(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>(bytes[0]) |
        (static_cast<std::uint16_t>(bytes[1]) << 8);
}

std::uint32_t read_u32(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
        (static_cast<std::uint32_t>(bytes[1]) << 8) |
        (static_cast<std::uint32_t>(bytes[2]) << 16) |
        (static_cast<std::uint32_t>(bytes[3]) << 24);
}

float read_f32(const std::uint8_t* bytes) {
    const std::uint32_t bits = read_u32(bytes);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool finite_sample(const Sample& sample) {
    return std::isfinite(sample.eye[0]) &&
        std::isfinite(sample.eye[1]) &&
        std::isfinite(sample.eye[2]) &&
        std::isfinite(sample.center[0]) &&
        std::isfinite(sample.center[1]) &&
        std::isfinite(sample.center[2]) &&
        std::isfinite(sample.fov) &&
        std::isfinite(sample.roll_degrees);
}

}  // namespace

std::uint32_t track_crc32(const std::uint8_t* bytes, std::uint32_t size) {
    std::uint32_t crc = 0xffffffffu;
    for (std::uint32_t index = 0; index < size; ++index) {
        std::uint8_t value = bytes[index];
        if (index >= 12 && index < 16) {
            value = 0;
        }
        crc ^= value;
        for (std::uint32_t bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^
                (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

TrackError validate_track(
    const void* data, std::uint32_t size, TrackView* output) {
    if (data == nullptr || output == nullptr) {
        return TrackError::NullInput;
    }
    if (size < kHeaderBytes) {
        return TrackError::HeaderTruncated;
    }
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    if (std::memcmp(bytes, "DPCM", 4) != 0) {
        return TrackError::Magic;
    }
    if (read_u16(bytes + 4) != 1) {
        return TrackError::Version;
    }
    if (read_u16(bytes + 6) != kHeaderBytes) {
        return TrackError::HeaderSize;
    }
    if (read_u32(bytes + 8) != size) {
        return TrackError::TotalSize;
    }
    if (read_u32(bytes + 12) != track_crc32(bytes, size)) {
        return TrackError::Crc;
    }
    const std::uint32_t source_fps = read_u32(bytes + 16);
    const std::uint32_t source_frames = read_u32(bytes + 20);
    const std::uint32_t sample_count = read_u32(bytes + 24);
    const std::uint32_t sample_stride = read_u32(bytes + 28);
    const std::uint32_t sample_offset = read_u32(bytes + 32);
    const std::uint32_t flags = read_u32(bytes + 36);
    if (source_fps != 30) {
        return TrackError::FrameRate;
    }
    if (source_frames == 0 || source_frames > kMaximumSourceFrames) {
        return TrackError::FrameCount;
    }
    if (sample_count != source_frames + 1) {
        return TrackError::SampleCount;
    }
    if (sample_stride != kSampleBytes) {
        return TrackError::SampleStride;
    }
    if (sample_offset < kHeaderBytes ||
        sample_offset > size ||
        sample_count > (size - sample_offset) / sample_stride ||
        sample_offset + sample_count * sample_stride != size) {
        return TrackError::SampleOffset;
    }
    if (flags != 1u) {
        return TrackError::Flags;
    }
    TrackView view = {
        bytes, size, source_fps, source_frames, sample_count, sample_offset};
    Sample sample = {};
    for (std::uint32_t frame = 0; frame < sample_count; ++frame) {
        const TrackError error = sample_source_frame(view, frame, &sample);
        if (error != TrackError::Ok) {
            return error;
        }
    }
    *output = view;
    return TrackError::Ok;
}

TrackError sample_source_frame(
    const TrackView& track, std::uint32_t source_frame, Sample* output) {
    if (track.bytes == nullptr || output == nullptr) {
        return TrackError::NullInput;
    }
    const std::uint32_t bounded =
        source_frame > track.source_frames ? track.source_frames : source_frame;
    const std::uint8_t* bytes =
        track.bytes + track.sample_offset + bounded * kSampleBytes;
    Sample sample = {};
    for (std::uint32_t axis = 0; axis < 3; ++axis) {
        sample.eye[axis] = read_f32(bytes + axis * 4);
        sample.center[axis] = read_f32(bytes + 12 + axis * 4);
    }
    sample.fov = read_f32(bytes + 24);
    sample.roll_degrees = read_f32(bytes + 28);
    if (!finite_sample(sample) || sample.fov <= 0.0f || sample.fov >= 180.0f) {
        return TrackError::NonFinite;
    }
    *output = sample;
    return TrackError::Ok;
}

const char* track_error_name(TrackError error) {
    switch (error) {
    case TrackError::Ok: return "ok";
    case TrackError::NullInput: return "null_input";
    case TrackError::HeaderTruncated: return "header_truncated";
    case TrackError::Magic: return "magic";
    case TrackError::Version: return "version";
    case TrackError::HeaderSize: return "header_size";
    case TrackError::TotalSize: return "total_size";
    case TrackError::Crc: return "crc";
    case TrackError::FrameRate: return "frame_rate";
    case TrackError::FrameCount: return "frame_count";
    case TrackError::SampleCount: return "sample_count";
    case TrackError::SampleStride: return "sample_stride";
    case TrackError::SampleOffset: return "sample_offset";
    case TrackError::Flags: return "flags";
    case TrackError::NonFinite: return "non_finite";
    }
    return "unknown";
}

}  // namespace dusk::psp::camera
