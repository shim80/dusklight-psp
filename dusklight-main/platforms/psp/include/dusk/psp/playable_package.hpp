#ifndef DUSK_PSP_PLAYABLE_PACKAGE_HPP
#define DUSK_PSP_PLAYABLE_PACKAGE_HPP

#include <cstddef>
#include <cstdint>

namespace dusk::psp::playable {

enum class PackageError : std::uint32_t {
    Ok,
    Missing,
    Truncated,
    Magic,
    Version,
    Size,
    Crc,
    Count,
    Range,
    JointParent,
    Weight,
    Index,
    Texture,
    Material,
    Animation,
    NonFinite,
    EdramBudget,
};

struct PackageView {
    const std::uint8_t* bytes;
    std::uint32_t size;
    std::uint32_t expected_crc;
    std::uint32_t actual_crc;
};

struct PackageSet {
    PackageView model;
    PackageView textures;
    PackageView animations;
    PackageView ui;
};

std::uint16_t read_u16(const std::uint8_t* bytes);
std::int16_t read_s16(const std::uint8_t* bytes);
std::uint32_t read_u32(const std::uint8_t* bytes);
float read_f32(const std::uint8_t* bytes);
std::uint32_t package_crc32(
    const std::uint8_t* bytes, std::uint32_t size);
PackageError validate_package(
    const void* bytes,
    std::uint32_t size,
    const char magic[4],
    PackageView* view);
PackageError validate_dpsk(
    const void* bytes, std::uint32_t size, PackageView* view);
PackageError validate_dptx(
    const void* bytes, std::uint32_t size, PackageView* view);
PackageError validate_dpan(
    const void* bytes, std::uint32_t size, PackageView* view);
PackageError validate_dpui(
    const void* bytes, std::uint32_t size, PackageView* view);
PackageError validate_package_set(const PackageSet& packages);
const char* package_error_name(PackageError error);

}  // namespace dusk::psp::playable

#endif
