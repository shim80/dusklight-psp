#ifndef DUSK_PSP_STARTUP_CAMERA_TRACK_HPP
#define DUSK_PSP_STARTUP_CAMERA_TRACK_HPP

#include <cstdint>

namespace dusk::psp::camera {

inline constexpr std::uint32_t kHeaderBytes = 64;
inline constexpr std::uint32_t kSampleBytes = 32;
inline constexpr std::uint32_t kMaximumSourceFrames = 60u * 60u * 10u;

enum class TrackError : std::uint8_t {
    Ok = 0,
    NullInput,
    HeaderTruncated,
    Magic,
    Version,
    HeaderSize,
    TotalSize,
    Crc,
    FrameRate,
    FrameCount,
    SampleCount,
    SampleStride,
    SampleOffset,
    Flags,
    NonFinite,
};

struct Sample {
    float eye[3];
    float center[3];
    float fov;
    float roll_degrees;
};

struct TrackView {
    const std::uint8_t* bytes;
    std::uint32_t size;
    std::uint32_t source_fps;
    std::uint32_t source_frames;
    std::uint32_t sample_count;
    std::uint32_t sample_offset;
};

std::uint32_t track_crc32(const std::uint8_t* bytes, std::uint32_t size);
TrackError validate_track(const void* data, std::uint32_t size, TrackView* output);
TrackError sample_source_frame(
    const TrackView& track, std::uint32_t source_frame, Sample* output);
const char* track_error_name(TrackError error);

}  // namespace dusk::psp::camera

#endif
