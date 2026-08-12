#ifndef DUSK_PSP_DPSM_HPP
#define DUSK_PSP_DPSM_HPP

#include <cstddef>
#include <cstdint>

namespace dusk::psp::dpsm {

inline constexpr std::uint32_t kHeaderBytes = 128;
inline constexpr std::uint32_t kMaximumPackageBytes = 2 * 1024 * 1024;
inline constexpr std::uint32_t kMaximumVertices = 65535;
inline constexpr std::uint32_t kMaximumIndices = 196605;
inline constexpr std::uint32_t kMaximumTextureDimension = 512;
inline constexpr std::uint32_t kMaximumTextureBytes = 1024 * 1024;
inline constexpr std::uint32_t kVertexStride = 20;
inline constexpr std::uint32_t kCrcOffset = 104;

enum class Error : std::uint32_t {
    Ok = 0,
    NullInput = 1,
    HeaderTruncated = 2,
    BadMagic = 3,
    UnsupportedVersion = 4,
    InvalidHeaderSize = 5,
    TotalSizeMismatch = 6,
    UnknownFlags = 7,
    ReservedNonZero = 8,
    UnknownVertexFormat = 9,
    UnknownIndexFormat = 10,
    UnknownTextureFormat = 11,
    InvalidVertexStride = 12,
    InvalidCount = 13,
    InvalidTriangleCount = 14,
    InvalidTextureDimensions = 15,
    SizeOverflow = 16,
    FileTooLarge = 17,
    MemoryBudgetExceeded = 18,
    OffsetOutOfRange = 19,
    SectionMisaligned = 20,
    SectionOverlap = 21,
    SectionSizeMismatch = 22,
    PaddingNonZero = 23,
    CrcMismatch = 24,
    NonFiniteVertex = 25,
    UvOutOfRange = 26,
    IndexOutOfRange = 27,
    BoundsInvalid = 28,
    BoundsMismatch = 29,
    IoOpenFailed = 30,
    IoSeekFailed = 31,
    IoReadFailed = 32,
    AllocationFailed = 33,
};

struct Vertex {
    float u;
    float v;
    float x;
    float y;
    float z;
};

static_assert(sizeof(Vertex) == kVertexStride);
static_assert(alignof(Vertex) == 4);

struct Bounds {
    float minimum[3];
    float maximum[3];
};

struct PackageView {
    const Vertex* vertices;
    const std::uint16_t* indices;
    const std::uint32_t* texture;
    std::uint32_t vertex_count;
    std::uint32_t index_count;
    std::uint32_t triangle_count;
    std::uint32_t texture_width;
    std::uint32_t texture_height;
    std::uint32_t texture_stride;
    std::uint32_t vertex_bytes;
    std::uint32_t index_bytes;
    std::uint32_t texture_bytes;
    Bounds bounds;
};

std::uint16_t read_u16_le(const std::uint8_t* bytes);
std::uint32_t read_u32_le(const std::uint8_t* bytes);
float read_f32_le(const std::uint8_t* bytes);
void write_u16_le(std::uint8_t* bytes, std::uint16_t value);
void write_u32_le(std::uint8_t* bytes, std::uint32_t value);
void write_f32_le(std::uint8_t* bytes, float value);

std::uint32_t crc32(const std::uint8_t* bytes, std::uint32_t size);
Error validate(
    const void* data,
    std::uint32_t size,
    std::uint32_t memory_budget,
    PackageView* output);
const char* error_name(Error error);

}  // namespace dusk::psp::dpsm

#endif
