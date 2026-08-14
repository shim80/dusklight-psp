#include "dusk/psp/startup_camera_track.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

void write_u16(std::uint8_t* bytes, std::uint16_t value) {
    bytes[0] = static_cast<std::uint8_t>(value);
    bytes[1] = static_cast<std::uint8_t>(value >> 8);
}

void write_u32(std::uint8_t* bytes, std::uint32_t value) {
    bytes[0] = static_cast<std::uint8_t>(value);
    bytes[1] = static_cast<std::uint8_t>(value >> 8);
    bytes[2] = static_cast<std::uint8_t>(value >> 16);
    bytes[3] = static_cast<std::uint8_t>(value >> 24);
}

void write_f32(std::uint8_t* bytes, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    write_u32(bytes, bits);
}

}  // namespace

int main() {
    using namespace dusk::psp::camera;
    constexpr std::uint32_t kSourceFrames = 2;
    constexpr std::uint32_t kSamples = kSourceFrames + 1;
    constexpr std::uint32_t kSize = kHeaderBytes + kSamples * kSampleBytes;
    std::array<std::uint8_t, kSize> bytes = {};
    std::memcpy(bytes.data(), "DPCM", 4);
    write_u16(bytes.data() + 4, 1);
    write_u16(bytes.data() + 6, static_cast<std::uint16_t>(kHeaderBytes));
    write_u32(bytes.data() + 8, kSize);
    write_u32(bytes.data() + 16, 30);
    write_u32(bytes.data() + 20, kSourceFrames);
    write_u32(bytes.data() + 24, kSamples);
    write_u32(bytes.data() + 28, kSampleBytes);
    write_u32(bytes.data() + 32, kHeaderBytes);
    write_u32(bytes.data() + 36, 1);

    for (std::uint32_t sample = 0; sample < kSamples; ++sample) {
        std::uint8_t* out = bytes.data() + kHeaderBytes + sample * kSampleBytes;
        write_f32(out + 0, 10.0f + static_cast<float>(sample));
        write_f32(out + 4, 20.0f);
        write_f32(out + 8, 30.0f);
        write_f32(out + 12, 40.0f);
        write_f32(out + 16, 50.0f + static_cast<float>(sample));
        write_f32(out + 20, 60.0f);
        write_f32(out + 24, 45.0f + static_cast<float>(sample));
        write_f32(out + 28, 5.0f * static_cast<float>(sample));
    }
    write_u32(bytes.data() + 12, track_crc32(bytes.data(), kSize));

    TrackView track = {};
    if (validate_track(bytes.data(), kSize, &track) != TrackError::Ok ||
        track.source_fps != 30 || track.source_frames != kSourceFrames ||
        track.sample_count != kSamples) {
        std::fprintf(stderr, "DPCM validation failed\n");
        return 1;
    }

    Sample sample = {};
    if (sample_source_frame(track, 1, &sample) != TrackError::Ok ||
        sample.eye[0] != 11.0f || sample.center[1] != 51.0f ||
        sample.fov != 46.0f || sample.roll_degrees != 5.0f) {
        std::fprintf(stderr, "DPCM sample mismatch\n");
        return 2;
    }

    if (sample_source_frame(track, 99, &sample) != TrackError::Ok ||
        sample.eye[0] != 12.0f || sample.roll_degrees != 10.0f) {
        std::fprintf(stderr, "DPCM end clamp failed\n");
        return 3;
    }

    auto corrupted = bytes;
    corrupted[kHeaderBytes + 3] ^= 0x80u;
    TrackView invalid = {};
    if (validate_track(corrupted.data(), kSize, &invalid) != TrackError::Crc) {
        std::fprintf(stderr, "DPCM corruption did not fail closed\n");
        return 4;
    }

    std::printf(
        "STARTUP_CAMERA_TRACK_HOST_OK source_fps=%u source_frames=%u "
        "samples=%u crc_fail_closed=1\n",
        track.source_fps, track.source_frames, track.sample_count);
    return 0;
}
