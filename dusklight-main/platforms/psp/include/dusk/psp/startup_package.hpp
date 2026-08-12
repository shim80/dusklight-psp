#ifndef DUSK_PSP_STARTUP_PACKAGE_HPP
#define DUSK_PSP_STARTUP_PACKAGE_HPP

#include <cstddef>
#include <cstdint>

namespace dusk::psp::startup {

inline constexpr std::uint32_t kPackageHeaderBytes = 64;
inline constexpr std::uint32_t kSegmentRecordBytes = 32;
inline constexpr std::uint32_t kMaximumSegments = 16;
inline constexpr std::uint32_t kMaximumPackageBytes =
    kPackageHeaderBytes + kMaximumSegments * kSegmentRecordBytes;
inline constexpr std::uint32_t kPackageCrcOffset = 28;

enum class Segment : std::uint8_t {
    BootWarning = 0,
    NintendoLogo = 1,
    DolbyLogo = 2,
    ProgressivePrompt = 3,
    OpeningLoad = 4,
    OpeningRealtime = 5,
    TitleLogo = 6,
    TitlePrompt = 7,
    FileSelect = 8,
    NewGameTransition = 9,
    UnsupportedGameplay = 10,
};

enum class AdvancePolicy : std::uint8_t {
    Timed = 0,
    TimedOrInput = 1,
    InputRequired = 2,
    ResourceReady = 3,
    SourceEvent = 4,
    UnsupportedBoundary = 5,
};

enum class Completeness : std::uint8_t {
    Complete = 0,
    Partial = 1,
    Unsupported = 2,
};

enum Capability : std::uint32_t {
    None = 0,
    Ui = 1u << 0,
    Stage = 1u << 1,
    Camera = 1u << 2,
    Events = 1u << 3,
    TitleModel = 1u << 4,
    FileSelection = 1u << 5,
    Gameplay = 1u << 6,
};

enum class PackageError : std::uint8_t {
    Ok = 0,
    NullInput,
    HeaderTruncated,
    BadMagic,
    UnsupportedVersion,
    InvalidHeaderSize,
    TotalSizeMismatch,
    FileTooLarge,
    UnknownFlags,
    ReservedNonZero,
    InvalidSegmentCount,
    InvalidRecordSize,
    InvalidOffset,
    InvalidSegment,
    InvalidPolicy,
    InvalidCompleteness,
    InvalidDuration,
    InvalidCapabilityMask,
    InvalidOrder,
    CrcMismatch,
};

struct SegmentRecord {
    Segment segment;
    AdvancePolicy policy;
    Completeness completeness;
    std::uint32_t duration_frames;
    std::uint32_t fade_in_frames;
    std::uint32_t fade_out_frames;
    std::uint32_t required_capabilities;
    std::uint32_t source_token;
};

struct PackageView {
    const std::uint8_t* bytes;
    std::uint32_t size;
    std::uint32_t segment_count;

    bool segment(std::uint32_t index, SegmentRecord* output) const;
};

std::uint32_t startup_crc32(
    const std::uint8_t* bytes,
    std::uint32_t size);
PackageError validate_startup_package(
    const void* data,
    std::uint32_t size,
    PackageView* output);
const char* package_error_name(PackageError error);
const char* segment_name(Segment segment);

}  // namespace dusk::psp::startup

#endif
