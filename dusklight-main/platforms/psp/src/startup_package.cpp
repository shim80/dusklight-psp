#include "dusk/psp/startup_package.hpp"

#include <cstring>

namespace dusk::psp::startup {
namespace {

constexpr std::uint16_t kVersion = 1;
constexpr std::uint32_t kKnownCapabilities =
    Capability::Ui | Capability::Stage | Capability::Camera |
    Capability::Events | Capability::TitleModel |
    Capability::FileSelection | Capability::Gameplay;

std::uint16_t read_u16(const std::uint8_t* source) {
    return static_cast<std::uint16_t>(source[0]) |
           static_cast<std::uint16_t>(source[1]) << 8;
}

std::uint32_t read_u32(const std::uint8_t* source) {
    return static_cast<std::uint32_t>(source[0]) |
           static_cast<std::uint32_t>(source[1]) << 8 |
           static_cast<std::uint32_t>(source[2]) << 16 |
           static_cast<std::uint32_t>(source[3]) << 24;
}

bool reserved_zero(
    const std::uint8_t* bytes,
    std::uint32_t begin,
    std::uint32_t end) {
    for (std::uint32_t offset = begin; offset < end; ++offset) {
        if (bytes[offset] != 0) {
            return false;
        }
    }
    return true;
}

}  // namespace

std::uint32_t startup_crc32(
    const std::uint8_t* bytes,
    std::uint32_t size) {
    if (bytes == nullptr) {
        return 0;
    }
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::uint32_t offset = 0; offset < size; ++offset) {
        const std::uint8_t value =
            offset >= kPackageCrcOffset &&
                    offset < kPackageCrcOffset + 4
                ? 0
                : bytes[offset];
        crc ^= value;
        for (std::uint32_t bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^
                  (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

bool PackageView::segment(
    std::uint32_t index,
    SegmentRecord* output) const {
    if (output == nullptr || bytes == nullptr || index >= segment_count) {
        return false;
    }
    const std::uint8_t* record =
        bytes + kPackageHeaderBytes + index * kSegmentRecordBytes;
    *output = {
        static_cast<Segment>(record[0]),
        static_cast<AdvancePolicy>(record[1]),
        static_cast<Completeness>(record[2]),
        read_u32(record + 4),
        read_u32(record + 8),
        read_u32(record + 12),
        read_u32(record + 16),
        read_u32(record + 20),
    };
    return true;
}

PackageError validate_startup_package(
    const void* data,
    std::uint32_t size,
    PackageView* output) {
    if (data == nullptr || output == nullptr) {
        return PackageError::NullInput;
    }
    *output = {};
    if (size < kPackageHeaderBytes) {
        return PackageError::HeaderTruncated;
    }
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    if (std::memcmp(bytes, "DPST", 4) != 0) {
        return PackageError::BadMagic;
    }
    if (read_u16(bytes + 4) != kVersion) {
        return PackageError::UnsupportedVersion;
    }
    if (read_u16(bytes + 6) != kPackageHeaderBytes) {
        return PackageError::InvalidHeaderSize;
    }
    if (read_u32(bytes + 8) != size) {
        return PackageError::TotalSizeMismatch;
    }
    if (size > kMaximumPackageBytes) {
        return PackageError::FileTooLarge;
    }
    const std::uint32_t count = read_u32(bytes + 12);
    if (count == 0 || count > kMaximumSegments) {
        return PackageError::InvalidSegmentCount;
    }
    if (read_u32(bytes + 16) != kPackageHeaderBytes) {
        return PackageError::InvalidOffset;
    }
    if (read_u32(bytes + 20) != kSegmentRecordBytes) {
        return PackageError::InvalidRecordSize;
    }
    if (read_u32(bytes + 24) != 0) {
        return PackageError::UnknownFlags;
    }
    if (!reserved_zero(bytes, 32, kPackageHeaderBytes)) {
        return PackageError::ReservedNonZero;
    }
    if (size != kPackageHeaderBytes + count * kSegmentRecordBytes) {
        return PackageError::TotalSizeMismatch;
    }

    std::uint8_t previous = 0;
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::uint8_t* record =
            bytes + kPackageHeaderBytes + index * kSegmentRecordBytes;
        if (record[0] >
            static_cast<std::uint8_t>(Segment::UnsupportedGameplay)) {
            return PackageError::InvalidSegment;
        }
        if (index != 0 && record[0] <= previous) {
            return PackageError::InvalidOrder;
        }
        previous = record[0];
        if (record[1] >
            static_cast<std::uint8_t>(
                AdvancePolicy::UnsupportedBoundary)) {
            return PackageError::InvalidPolicy;
        }
        if (record[2] >
            static_cast<std::uint8_t>(Completeness::Unsupported)) {
            return PackageError::InvalidCompleteness;
        }
        if (record[3] != 0 ||
            !reserved_zero(record, 24, kSegmentRecordBytes)) {
            return PackageError::ReservedNonZero;
        }
        const auto policy = static_cast<AdvancePolicy>(record[1]);
        const std::uint32_t duration = read_u32(record + 4);
        if ((policy == AdvancePolicy::Timed ||
             policy == AdvancePolicy::TimedOrInput) &&
            duration == 0) {
            return PackageError::InvalidDuration;
        }
        if ((read_u32(record + 16) & ~kKnownCapabilities) != 0) {
            return PackageError::InvalidCapabilityMask;
        }
    }
    if (read_u32(bytes + kPackageCrcOffset) !=
        startup_crc32(bytes, size)) {
        return PackageError::CrcMismatch;
    }
    *output = {bytes, size, count};
    return PackageError::Ok;
}

const char* package_error_name(PackageError error) {
    static constexpr const char* kNames[] = {
        "ok", "null_input", "header_truncated", "bad_magic",
        "unsupported_version", "invalid_header_size",
        "total_size_mismatch", "file_too_large", "unknown_flags",
        "reserved_non_zero", "invalid_segment_count",
        "invalid_record_size", "invalid_offset", "invalid_segment",
        "invalid_policy", "invalid_completeness", "invalid_duration",
        "invalid_capability_mask", "invalid_order", "crc_mismatch",
    };
    const auto index = static_cast<std::uint8_t>(error);
    return index < sizeof(kNames) / sizeof(kNames[0])
        ? kNames[index]
        : "unknown";
}

const char* segment_name(Segment segment) {
    static constexpr const char* kNames[] = {
        "boot_warning",
        "nintendo_logo",
        "dolby_logo",
        "progressive_prompt",
        "opening_stage_load",
        "opening_realtime",
        "title_logo",
        "title_prompt",
        "file_select",
        "new_game_transition",
        "unsupported_gameplay",
    };
    const auto index = static_cast<std::uint8_t>(segment);
    return index < sizeof(kNames) / sizeof(kNames[0])
        ? kNames[index]
        : "invalid";
}

}  // namespace dusk::psp::startup
