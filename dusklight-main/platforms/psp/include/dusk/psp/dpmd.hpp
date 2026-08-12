#ifndef DUSK_PSP_DPMD_HPP
#define DUSK_PSP_DPMD_HPP

#include <cstddef>
#include <cstdint>

namespace dusk::psp::dpmd {

inline constexpr std::uint32_t kHeaderBytes = 128;
inline constexpr std::uint32_t kChunkBytes = 96;
inline constexpr std::uint32_t kCrcOffset = 104;
inline constexpr std::uint32_t kVertexStride = 16;
inline constexpr std::uint32_t kMaximumPackageBytes = 2 * 1024 * 1024;
inline constexpr std::uint32_t kMaximumChunks = 16;
inline constexpr std::uint32_t kMaximumVerticesPerChunk = 65535;
inline constexpr std::uint32_t kMaximumIndicesPerChunk = 196605;

enum class Error : std::uint32_t {
    Ok = 0,
    NullInput,
    HeaderTruncated,
    BadMagic,
    UnsupportedVersion,
    InvalidHeaderSize,
    TotalSizeMismatch,
    UnknownFlags,
    InvalidCount,
    InvalidTriangleCount,
    InvalidFormat,
    InvalidVertexStride,
    SizeOverflow,
    FileTooLarge,
    MemoryBudgetExceeded,
    OffsetOutOfRange,
    SectionMisaligned,
    SectionOverlap,
    SectionSizeMismatch,
    PaddingNonZero,
    CrcMismatch,
    NonFiniteVertex,
    IndexOutOfRange,
    BoundsInvalid,
    BoundsMismatch,
    DuplicatePart,
    IoOpenFailed,
    IoSeekFailed,
    IoReadFailed,
    AllocationFailed,
};

enum class Part : std::uint32_t {
    Body = 1,
    Head = 2,
    Face = 3,
    LeftHand = 4,
    RightHand = 5,
};

struct Vertex {
    std::uint32_t color;
    float x;
    float y;
    float z;
};

static_assert(sizeof(Vertex) == kVertexStride);
static_assert(offsetof(Vertex, color) == 0);
static_assert(offsetof(Vertex, x) == 4);

struct Bounds {
    float minimum[3];
    float maximum[3];
};

struct ChunkView {
    Part part;
    std::uint32_t source_hash;
    const Vertex* vertices;
    const std::uint16_t* indices;
    std::uint32_t vertex_count;
    std::uint32_t index_count;
    std::uint32_t triangle_count;
    std::uint32_t vertex_bytes;
    std::uint32_t index_bytes;
    Bounds bounds;
};

struct PackageView {
    const void* storage;
    std::uint32_t storage_bytes;
    std::uint32_t chunk_count;
    std::uint32_t vertex_count;
    std::uint32_t index_count;
    std::uint32_t triangle_count;
    std::uint32_t expected_crc;
    std::uint32_t actual_crc;
    Bounds bounds;
    ChunkView chunks[kMaximumChunks];
};

std::uint16_t read_u16_le(const std::uint8_t* bytes);
std::uint32_t read_u32_le(const std::uint8_t* bytes);
float read_f32_le(const std::uint8_t* bytes);
std::uint32_t crc32(const std::uint8_t* bytes, std::uint32_t size);
Error validate(
    const void* data,
    std::uint32_t size,
    std::uint32_t memory_budget,
    PackageView* output);
const char* error_name(Error error);

}  // namespace dusk::psp::dpmd

#endif
