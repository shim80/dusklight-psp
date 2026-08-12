#include "dusk/psp/dpmd.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace dusk::psp::dpmd {
namespace {

constexpr float kBoundsTolerance = 0.002f;

bool add_overflows(std::uint32_t left, std::uint32_t right) {
    return left > std::numeric_limits<std::uint32_t>::max() - right;
}

bool finite_bounds(const Bounds& bounds) {
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(bounds.minimum[axis]) ||
            !std::isfinite(bounds.maximum[axis]) ||
            bounds.minimum[axis] > bounds.maximum[axis]) {
            return false;
        }
    }
    return true;
}

bool close(float left, float right) {
    return std::fabs(left - right) <= kBoundsTolerance;
}

}  // namespace

std::uint16_t read_u16_le(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>(bytes[0]) |
           static_cast<std::uint16_t>(bytes[1]) << 8;
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

std::uint32_t crc32(const std::uint8_t* bytes, std::uint32_t size) {
    if (bytes == nullptr) {
        return 0;
    }
    std::uint32_t crc = 0xffffffffu;
    for (std::uint32_t offset = 0; offset < size; ++offset) {
        const std::uint8_t byte =
            offset >= kCrcOffset && offset < kCrcOffset + 4
                ? 0
                : bytes[offset];
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^
                  (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
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
    if (size > kMaximumPackageBytes) {
        return Error::FileTooLarge;
    }
    if (memory_budget == 0 || size > memory_budget) {
        return Error::MemoryBudgetExceeded;
    }
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    if (std::memcmp(bytes, "DPMD", 4) != 0) {
        return Error::BadMagic;
    }
    if (read_u16_le(bytes + 4) != 1) {
        return Error::UnsupportedVersion;
    }
    if (read_u16_le(bytes + 6) != kHeaderBytes) {
        return Error::InvalidHeaderSize;
    }
    if (read_u32_le(bytes + 8) != size) {
        return Error::TotalSizeMismatch;
    }
    if (read_u32_le(bytes + 12) != 0x3u) {
        return Error::UnknownFlags;
    }
    const std::uint32_t chunk_count = read_u32_le(bytes + 16);
    const std::uint32_t total_vertices = read_u32_le(bytes + 20);
    const std::uint32_t total_indices = read_u32_le(bytes + 24);
    const std::uint32_t total_triangles = read_u32_le(bytes + 28);
    if (chunk_count != 5 || chunk_count > kMaximumChunks ||
        total_vertices == 0 || total_indices == 0) {
        return Error::InvalidCount;
    }
    if (total_indices % 3 != 0 ||
        total_triangles != total_indices / 3) {
        return Error::InvalidTriangleCount;
    }
    if (read_u32_le(bytes + 32) != 1 ||
        read_u32_le(bytes + 36) != 1) {
        return Error::InvalidFormat;
    }
    if (read_u32_le(bytes + 40) != kVertexStride) {
        return Error::InvalidVertexStride;
    }
    for (std::uint32_t offset = 44; offset < 48; ++offset) {
        if (bytes[offset] != 0) {
            return Error::PaddingNonZero;
        }
    }
    Bounds package_bounds = {};
    for (int axis = 0; axis < 3; ++axis) {
        package_bounds.minimum[axis] = read_f32_le(bytes + 48 + axis * 4);
        package_bounds.maximum[axis] = read_f32_le(bytes + 60 + axis * 4);
    }
    if (!finite_bounds(package_bounds)) {
        return Error::BoundsInvalid;
    }
    const std::uint32_t table_offset = read_u32_le(bytes + 88);
    const std::uint32_t table_bytes = read_u32_le(bytes + 92);
    const std::uint32_t data_offset = read_u32_le(bytes + 96);
    const std::uint32_t data_bytes = read_u32_le(bytes + 100);
    if ((table_offset & 15u) != 0 || (data_offset & 15u) != 0) {
        return Error::SectionMisaligned;
    }
    if (table_offset != kHeaderBytes ||
        table_bytes != chunk_count * kChunkBytes ||
        add_overflows(table_offset, table_bytes) ||
        table_offset + table_bytes > data_offset ||
        add_overflows(data_offset, data_bytes) ||
        data_offset + data_bytes != size) {
        return Error::SectionSizeMismatch;
    }
    for (std::uint32_t offset = 120; offset < kHeaderBytes; ++offset) {
        if (bytes[offset] != 0) {
            return Error::PaddingNonZero;
        }
    }
    const std::uint32_t expected_crc = read_u32_le(bytes + kCrcOffset);
    const std::uint32_t actual_crc = crc32(bytes, size);
    if (expected_crc != actual_crc) {
        return Error::CrcMismatch;
    }

    std::uint32_t seen_parts = 0;
    std::uint32_t summed_vertices = 0;
    std::uint32_t summed_indices = 0;
    Bounds measured = {
        {INFINITY, INFINITY, INFINITY},
        {-INFINITY, -INFINITY, -INFINITY},
    };
    for (std::uint32_t index = 0; index < chunk_count; ++index) {
        const std::uint8_t* entry =
            bytes + table_offset + index * kChunkBytes;
        const std::uint32_t part_value = read_u32_le(entry);
        if (part_value < 1 || part_value > 5) {
            return Error::InvalidCount;
        }
        const std::uint32_t part_bit = 1u << (part_value - 1);
        if ((seen_parts & part_bit) != 0) {
            return Error::DuplicatePart;
        }
        seen_parts |= part_bit;
        if (read_u32_le(entry + 8) != 0 ||
            read_u32_le(entry + 64) != 1) {
            return Error::InvalidFormat;
        }
        for (std::uint32_t offset = 68; offset < kChunkBytes; ++offset) {
            if (entry[offset] != 0) {
                return Error::PaddingNonZero;
            }
        }
        const std::uint32_t vertex_offset = read_u32_le(entry + 12);
        const std::uint32_t vertex_count = read_u32_le(entry + 16);
        const std::uint32_t vertex_bytes = read_u32_le(entry + 20);
        const std::uint32_t index_offset = read_u32_le(entry + 24);
        const std::uint32_t index_count = read_u32_le(entry + 28);
        const std::uint32_t index_bytes = read_u32_le(entry + 32);
        const std::uint32_t triangle_count = read_u32_le(entry + 36);
        if (vertex_count == 0 ||
            vertex_count > kMaximumVerticesPerChunk ||
            index_count == 0 ||
            index_count > kMaximumIndicesPerChunk) {
            return Error::InvalidCount;
        }
        if (index_count % 3 != 0 ||
            triangle_count != index_count / 3) {
            return Error::InvalidTriangleCount;
        }
        if (vertex_bytes != vertex_count * kVertexStride ||
            index_bytes != index_count * sizeof(std::uint16_t)) {
            return Error::SectionSizeMismatch;
        }
        if ((vertex_offset & 15u) != 0 || (index_offset & 15u) != 0) {
            return Error::SectionMisaligned;
        }
        if (vertex_offset < data_offset ||
            index_offset < data_offset ||
            add_overflows(vertex_offset, vertex_bytes) ||
            add_overflows(index_offset, index_bytes) ||
            vertex_offset + vertex_bytes > size ||
            index_offset + index_bytes > size ||
            (vertex_offset < index_offset + index_bytes &&
             index_offset < vertex_offset + vertex_bytes)) {
            return Error::OffsetOutOfRange;
        }
        Bounds chunk_bounds = {};
        for (int axis = 0; axis < 3; ++axis) {
            chunk_bounds.minimum[axis] =
                read_f32_le(entry + 40 + axis * 4);
            chunk_bounds.maximum[axis] =
                read_f32_le(entry + 52 + axis * 4);
        }
        if (!finite_bounds(chunk_bounds)) {
            return Error::BoundsInvalid;
        }
        const auto* vertices =
            reinterpret_cast<const Vertex*>(bytes + vertex_offset);
        const auto* indices =
            reinterpret_cast<const std::uint16_t*>(bytes + index_offset);
        Bounds chunk_measured = {
            {INFINITY, INFINITY, INFINITY},
            {-INFINITY, -INFINITY, -INFINITY},
        };
        for (std::uint32_t vertex = 0; vertex < vertex_count; ++vertex) {
            const float values[3] = {
                vertices[vertex].x,
                vertices[vertex].y,
                vertices[vertex].z,
            };
            for (int axis = 0; axis < 3; ++axis) {
                if (!std::isfinite(values[axis])) {
                    return Error::NonFiniteVertex;
                }
                chunk_measured.minimum[axis] =
                    std::min(chunk_measured.minimum[axis], values[axis]);
                chunk_measured.maximum[axis] =
                    std::max(chunk_measured.maximum[axis], values[axis]);
            }
        }
        for (std::uint32_t item = 0; item < index_count; ++item) {
            if (indices[item] >= vertex_count) {
                return Error::IndexOutOfRange;
            }
        }
        for (int axis = 0; axis < 3; ++axis) {
            if (!close(chunk_bounds.minimum[axis],
                       chunk_measured.minimum[axis]) ||
                !close(chunk_bounds.maximum[axis],
                       chunk_measured.maximum[axis])) {
                return Error::BoundsMismatch;
            }
            measured.minimum[axis] =
                std::min(measured.minimum[axis], chunk_measured.minimum[axis]);
            measured.maximum[axis] =
                std::max(measured.maximum[axis], chunk_measured.maximum[axis]);
        }
        summed_vertices += vertex_count;
        summed_indices += index_count;
        output->chunks[index] = {
            static_cast<Part>(part_value),
            read_u32_le(entry + 4),
            vertices,
            indices,
            vertex_count,
            index_count,
            triangle_count,
            vertex_bytes,
            index_bytes,
            chunk_bounds,
        };
    }
    if (seen_parts != 0x1fu ||
        summed_vertices != total_vertices ||
        summed_indices != total_indices) {
        return Error::InvalidCount;
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (!close(package_bounds.minimum[axis], measured.minimum[axis]) ||
            !close(package_bounds.maximum[axis], measured.maximum[axis])) {
            return Error::BoundsMismatch;
        }
    }
    output->storage = data;
    output->storage_bytes = size;
    output->chunk_count = chunk_count;
    output->vertex_count = total_vertices;
    output->index_count = total_indices;
    output->triangle_count = total_triangles;
    output->expected_crc = expected_crc;
    output->actual_crc = actual_crc;
    output->bounds = package_bounds;
    return Error::Ok;
}

const char* error_name(Error error) {
    static constexpr const char* names[] = {
        "Ok", "NullInput", "HeaderTruncated", "BadMagic",
        "UnsupportedVersion", "InvalidHeaderSize", "TotalSizeMismatch",
        "UnknownFlags", "InvalidCount", "InvalidTriangleCount",
        "InvalidFormat", "InvalidVertexStride", "SizeOverflow",
        "FileTooLarge", "MemoryBudgetExceeded", "OffsetOutOfRange",
        "SectionMisaligned", "SectionOverlap", "SectionSizeMismatch",
        "PaddingNonZero", "CrcMismatch", "NonFiniteVertex",
        "IndexOutOfRange", "BoundsInvalid", "BoundsMismatch",
        "DuplicatePart", "IoOpenFailed", "IoSeekFailed", "IoReadFailed",
        "AllocationFailed",
    };
    const auto index = static_cast<std::uint32_t>(error);
    return index < sizeof(names) / sizeof(names[0])
               ? names[index]
               : "Unknown";
}

}  // namespace dusk::psp::dpmd
