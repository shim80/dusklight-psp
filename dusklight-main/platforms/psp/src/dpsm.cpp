#include "dusk/psp/dpsm.hpp"

#include <cmath>
#include <cstring>

namespace dusk::psp::dpsm {
namespace {

constexpr std::uint32_t kVersion = 1;
constexpr std::uint32_t kVertexFormat = 1;
constexpr std::uint32_t kIndexFormat = 1;
constexpr std::uint32_t kTextureFormat = 1;
constexpr float kBoundsTolerance = 1.0e-6f;

bool checked_multiply(
    std::uint32_t left,
    std::uint32_t right,
    std::uint32_t* output) {
    if (output == nullptr ||
        (left != 0 && right > UINT32_MAX / left)) {
        return false;
    }
    *output = left * right;
    return true;
}

bool range_valid(
    std::uint32_t offset,
    std::uint32_t bytes,
    std::uint32_t total) {
    return offset <= total && bytes <= total - offset;
}

bool padding_zero(
    const std::uint8_t* data,
    std::uint32_t begin,
    std::uint32_t end) {
    for (std::uint32_t index = begin; index < end; ++index) {
        if (data[index] != 0) {
            return false;
        }
    }
    return true;
}

bool nearly_equal(float left, float right) {
    return std::fabs(left - right) <= kBoundsTolerance;
}

}  // namespace

std::uint16_t read_u16_le(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(bytes[0]) |
        static_cast<std::uint16_t>(bytes[1]) << 8);
}

std::uint32_t read_u32_le(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           static_cast<std::uint32_t>(bytes[1]) << 8 |
           static_cast<std::uint32_t>(bytes[2]) << 16 |
           static_cast<std::uint32_t>(bytes[3]) << 24;
}

float read_f32_le(const std::uint8_t* bytes) {
    const std::uint32_t bits = read_u32_le(bytes);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void write_u16_le(std::uint8_t* bytes, std::uint16_t value) {
    bytes[0] = static_cast<std::uint8_t>(value);
    bytes[1] = static_cast<std::uint8_t>(value >> 8);
}

void write_u32_le(std::uint8_t* bytes, std::uint32_t value) {
    bytes[0] = static_cast<std::uint8_t>(value);
    bytes[1] = static_cast<std::uint8_t>(value >> 8);
    bytes[2] = static_cast<std::uint8_t>(value >> 16);
    bytes[3] = static_cast<std::uint8_t>(value >> 24);
}

void write_f32_le(std::uint8_t* bytes, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    write_u32_le(bytes, bits);
}

std::uint32_t crc32(const std::uint8_t* bytes, std::uint32_t size) {
    if (bytes == nullptr) {
        return 0;
    }
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::uint32_t offset = 0; offset < size; ++offset) {
        const std::uint8_t byte =
            offset >= kCrcOffset && offset < kCrcOffset + 4
                ? 0
                : bytes[offset];
        crc ^= byte;
        for (std::uint32_t bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^
                  (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

Error validate(
    const void* data,
    std::uint32_t size,
    std::uint32_t memory_budget,
    PackageView* output) {
    if (data == nullptr || output == nullptr) {
        return Error::NullInput;
    }
    *output = {};
    if (size < kHeaderBytes) {
        return Error::HeaderTruncated;
    }
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    if (std::memcmp(bytes, "DPSM", 4) != 0) {
        return Error::BadMagic;
    }
    if (read_u16_le(bytes + 4) != kVersion) {
        return Error::UnsupportedVersion;
    }
    if (read_u16_le(bytes + 6) != kHeaderBytes) {
        return Error::InvalidHeaderSize;
    }
    const std::uint32_t total_bytes = read_u32_le(bytes + 8);
    if (total_bytes != size) {
        return Error::TotalSizeMismatch;
    }
    if (total_bytes > kMaximumPackageBytes) {
        return Error::FileTooLarge;
    }
    if (memory_budget == 0 || total_bytes > memory_budget) {
        return Error::MemoryBudgetExceeded;
    }
    if (read_u32_le(bytes + 12) != 0) {
        return Error::UnknownFlags;
    }
    if (read_u32_le(bytes + 16) != kVertexFormat) {
        return Error::UnknownVertexFormat;
    }
    if (read_u32_le(bytes + 20) != kIndexFormat) {
        return Error::UnknownIndexFormat;
    }
    if (read_u32_le(bytes + 24) != kTextureFormat) {
        return Error::UnknownTextureFormat;
    }
    if (read_u32_le(bytes + 28) != kVertexStride) {
        return Error::InvalidVertexStride;
    }
    for (std::uint32_t offset = 108; offset < kHeaderBytes; offset += 4) {
        if (read_u32_le(bytes + offset) != 0) {
            return Error::ReservedNonZero;
        }
    }

    const std::uint32_t vertex_count = read_u32_le(bytes + 32);
    const std::uint32_t index_count = read_u32_le(bytes + 36);
    const std::uint32_t triangle_count = read_u32_le(bytes + 40);
    if (vertex_count == 0 || vertex_count > kMaximumVertices ||
        index_count == 0 || index_count > kMaximumIndices ||
        index_count % 3 != 0) {
        return Error::InvalidCount;
    }
    if (triangle_count != index_count / 3) {
        return Error::InvalidTriangleCount;
    }

    const std::uint32_t width = read_u32_le(bytes + 44);
    const std::uint32_t height = read_u32_le(bytes + 48);
    const std::uint32_t stride = read_u32_le(bytes + 52);
    if (width == 0 || height == 0 ||
        width > kMaximumTextureDimension ||
        height > kMaximumTextureDimension || stride != width) {
        return Error::InvalidTextureDimensions;
    }

    std::uint32_t expected_vertex_bytes = 0;
    std::uint32_t expected_index_bytes = 0;
    std::uint32_t texture_pixels = 0;
    std::uint32_t expected_texture_bytes = 0;
    if (!checked_multiply(vertex_count, kVertexStride,
                          &expected_vertex_bytes) ||
        !checked_multiply(index_count, 2, &expected_index_bytes) ||
        !checked_multiply(width, height, &texture_pixels) ||
        !checked_multiply(texture_pixels, 4, &expected_texture_bytes)) {
        return Error::SizeOverflow;
    }
    if (expected_texture_bytes > kMaximumTextureBytes) {
        return Error::InvalidTextureDimensions;
    }

    const std::uint32_t vertex_offset = read_u32_le(bytes + 56);
    const std::uint32_t vertex_bytes = read_u32_le(bytes + 60);
    const std::uint32_t index_offset = read_u32_le(bytes + 64);
    const std::uint32_t index_bytes = read_u32_le(bytes + 68);
    const std::uint32_t texture_offset = read_u32_le(bytes + 72);
    const std::uint32_t texture_bytes = read_u32_le(bytes + 76);
    if (vertex_bytes != expected_vertex_bytes ||
        index_bytes != expected_index_bytes ||
        texture_bytes != expected_texture_bytes) {
        return Error::SectionSizeMismatch;
    }
    if ((vertex_offset & 15u) != 0 || (index_offset & 15u) != 0 ||
        (texture_offset & 15u) != 0 || (total_bytes & 15u) != 0) {
        return Error::SectionMisaligned;
    }
    if (!range_valid(vertex_offset, vertex_bytes, total_bytes) ||
        !range_valid(index_offset, index_bytes, total_bytes) ||
        !range_valid(texture_offset, texture_bytes, total_bytes) ||
        vertex_offset < kHeaderBytes) {
        return Error::OffsetOutOfRange;
    }
    const std::uint32_t vertex_end = vertex_offset + vertex_bytes;
    const std::uint32_t index_end = index_offset + index_bytes;
    const std::uint32_t texture_end = texture_offset + texture_bytes;
    if (vertex_end > index_offset || index_end > texture_offset) {
        return Error::SectionOverlap;
    }
    if (!padding_zero(bytes, kHeaderBytes, vertex_offset) ||
        !padding_zero(bytes, vertex_end, index_offset) ||
        !padding_zero(bytes, index_end, texture_offset) ||
        !padding_zero(bytes, texture_end, total_bytes)) {
        return Error::PaddingNonZero;
    }
    if (read_u32_le(bytes + kCrcOffset) != crc32(bytes, total_bytes)) {
        return Error::CrcMismatch;
    }

    Bounds bounds = {};
    for (std::uint32_t axis = 0; axis < 3; ++axis) {
        bounds.minimum[axis] = read_f32_le(bytes + 80 + axis * 4);
        bounds.maximum[axis] = read_f32_le(bytes + 92 + axis * 4);
        if (!std::isfinite(bounds.minimum[axis]) ||
            !std::isfinite(bounds.maximum[axis]) ||
            bounds.minimum[axis] > bounds.maximum[axis]) {
            return Error::BoundsInvalid;
        }
    }

    float calculated_minimum[3] = {};
    float calculated_maximum[3] = {};
    for (std::uint32_t index = 0; index < vertex_count; ++index) {
        const std::uint8_t* source =
            bytes + vertex_offset + index * kVertexStride;
        const float values[5] = {
            read_f32_le(source),
            read_f32_le(source + 4),
            read_f32_le(source + 8),
            read_f32_le(source + 12),
            read_f32_le(source + 16),
        };
        for (float value : values) {
            if (!std::isfinite(value)) {
                return Error::NonFiniteVertex;
            }
        }
        if (values[0] < 0.0f || values[0] > 1.0f ||
            values[1] < 0.0f || values[1] > 1.0f) {
            return Error::UvOutOfRange;
        }
        for (std::uint32_t axis = 0; axis < 3; ++axis) {
            const float value = values[axis + 2];
            if (index == 0 || value < calculated_minimum[axis]) {
                calculated_minimum[axis] = value;
            }
            if (index == 0 || value > calculated_maximum[axis]) {
                calculated_maximum[axis] = value;
            }
        }
    }
    for (std::uint32_t axis = 0; axis < 3; ++axis) {
        if (!nearly_equal(bounds.minimum[axis], calculated_minimum[axis]) ||
            !nearly_equal(bounds.maximum[axis], calculated_maximum[axis])) {
            return Error::BoundsMismatch;
        }
    }
    for (std::uint32_t index = 0; index < index_count; ++index) {
        if (read_u16_le(bytes + index_offset + index * 2) >= vertex_count) {
            return Error::IndexOutOfRange;
        }
    }

    if ((reinterpret_cast<std::uintptr_t>(bytes + vertex_offset) & 3u) != 0 ||
        (reinterpret_cast<std::uintptr_t>(bytes + index_offset) & 1u) != 0 ||
        (reinterpret_cast<std::uintptr_t>(bytes + texture_offset) & 3u) != 0) {
        return Error::SectionMisaligned;
    }
    output->vertices =
        reinterpret_cast<const Vertex*>(bytes + vertex_offset);
    output->indices =
        reinterpret_cast<const std::uint16_t*>(bytes + index_offset);
    output->texture =
        reinterpret_cast<const std::uint32_t*>(bytes + texture_offset);
    output->vertex_count = vertex_count;
    output->index_count = index_count;
    output->triangle_count = triangle_count;
    output->texture_width = width;
    output->texture_height = height;
    output->texture_stride = stride;
    output->vertex_bytes = vertex_bytes;
    output->index_bytes = index_bytes;
    output->texture_bytes = texture_bytes;
    output->bounds = bounds;
    return Error::Ok;
}

const char* error_name(Error error) {
    static constexpr const char* kNames[] = {
        "Ok", "NullInput", "HeaderTruncated", "BadMagic",
        "UnsupportedVersion", "InvalidHeaderSize", "TotalSizeMismatch",
        "UnknownFlags", "ReservedNonZero", "UnknownVertexFormat",
        "UnknownIndexFormat", "UnknownTextureFormat", "InvalidVertexStride",
        "InvalidCount", "InvalidTriangleCount", "InvalidTextureDimensions",
        "SizeOverflow", "FileTooLarge", "MemoryBudgetExceeded",
        "OffsetOutOfRange", "SectionMisaligned", "SectionOverlap",
        "SectionSizeMismatch", "PaddingNonZero", "CrcMismatch",
        "NonFiniteVertex", "UvOutOfRange", "IndexOutOfRange",
        "BoundsInvalid", "BoundsMismatch", "IoOpenFailed", "IoSeekFailed",
        "IoReadFailed", "AllocationFailed",
    };
    const auto index = static_cast<std::uint32_t>(error);
    return index < sizeof(kNames) / sizeof(kNames[0])
               ? kNames[index]
               : "UnknownError";
}

}  // namespace dusk::psp::dpsm
