#include "dusk/psp/room_package.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

void put_u16(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put_u32(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(
            value >> (index * 8));
    }
}

std::vector<std::uint8_t> make_package() {
    constexpr std::uint32_t kTextureTable = 128;
    constexpr std::uint32_t kMaterialTable = 224;
    constexpr std::uint32_t kPixels = 256;
    constexpr std::uint32_t kPeTable = 512;
    constexpr std::uint32_t kPlanTable = 560;
    constexpr std::uint32_t kSize = 608;
    std::vector<std::uint8_t> bytes(kSize, 0);
    bytes[0] = 'D'; bytes[1] = 'P'; bytes[2] = 'T'; bytes[3] = 'X';
    put_u16(bytes, 4, 3);
    put_u16(bytes, 6, 128);
    put_u32(bytes, 8, kSize);
    put_u32(bytes, 16, 2);
    put_u32(bytes, 20, 1);
    put_u32(bytes, 24, kTextureTable);
    put_u32(bytes, 28, 48);
    put_u32(bytes, 32, kMaterialTable);
    put_u32(bytes, 36, 32);
    put_u32(bytes, 48, 256);
    put_u32(bytes, 64, 0x31564550u);  // PEV1
    put_u32(bytes, 68, kPeTable);
    put_u32(bytes, 72, 40);
    put_u32(bytes, 76, 1);
    put_u32(bytes, 80, 1);
    put_u32(bytes, 84, 0x3156504du);  // MPV1
    put_u32(bytes, 88, kPlanTable);
    put_u32(bytes, 92, 48);
    put_u32(bytes, 96, 1);
    put_u32(bytes, 100, 1);

    for (std::uint32_t index = 0; index < 2; ++index) {
        const std::uint32_t item = kTextureTable + index * 48;
        put_u32(bytes, item, index);
        put_u16(bytes, item + 4, 8);
        put_u16(bytes, item + 6, 8);
        put_u16(bytes, item + 8, 8);
        put_u16(bytes, item + 10, 8);
        bytes[item + 12] = 1;
        put_u32(bytes, item + 16, kPixels + index * 128);
        put_u32(bytes, item + 20, 128);
    }

    put_u16(bytes, kMaterialTable, 0);
    bytes[kMaterialTable + 4] = 0xff;
    bytes[kMaterialTable + 5] = 0xff;
    bytes[kMaterialTable + 6] = 0xff;
    bytes[kMaterialTable + 7] = 0xff;
    bytes[kMaterialTable + 8] = 0xff;
    bytes[kMaterialTable + 9] = 0xff;
    bytes[kMaterialTable + 10] = 0xff;
    bytes[kMaterialTable + 11] = 0xff;

    put_u16(bytes, kPeTable, 0);
    bytes[kPeTable + 4] = 1;
    bytes[kPeTable + 5] = 3;
    bytes[kPeTable + 6] = 1;
    bytes[kPeTable + 8] = 7;
    bytes[kPeTable + 11] = 7;
    bytes[kPeTable + 13] = 0;
    bytes[kPeTable + 17] = 2;
    bytes[kPeTable + 18] = 1;
    bytes[kPeTable + 19] = 1;
    put_u16(bytes, kPeTable + 20, 0);
    put_u16(bytes, kPeTable + 22, 1);
    for (std::size_t index = 2; index < 8; ++index) {
        put_u16(bytes, kPeTable + 20 + index * 2, 0xffffu);
    }

    put_u16(bytes, kPlanTable, 0);
    bytes[kPlanTable + 2] = 1;  // Approximate
    bytes[kPlanTable + 3] = 2;  // UnsupportedTexgen
    bytes[kPlanTable + 4] = 2;
    put_u16(bytes, kPlanTable + 8, 0);
    bytes[kPlanTable + 10] = 0;  // Modulate
    bytes[kPlanTable + 11] = 1;  // RGBA
    bytes[kPlanTable + 12] = 0;  // Source blend
    bytes[kPlanTable + 13] = 3;  // depth write + texture
    put_u32(bytes, kPlanTable + 16, 0xffffffffu);
    put_u16(bytes, kPlanTable + 28, 1);
    bytes[kPlanTable + 30] = 1;  // Replace
    bytes[kPlanTable + 31] = 1;  // RGBA
    bytes[kPlanTable + 32] = 4;  // Multiply
    bytes[kPlanTable + 33] = 2;  // texture, no depth write
    put_u32(bytes, kPlanTable + 36, 0xffffffffu);

    put_u32(bytes, 12, dusk::psp::room::package_crc32(
        bytes.data(), static_cast<std::uint32_t>(bytes.size())));
    return bytes;
}

void refresh_crc(std::vector<std::uint8_t>& package) {
    put_u32(package, 12, 0);
    put_u32(package, 12, dusk::psp::room::package_crc32(
        package.data(), static_cast<std::uint32_t>(package.size())));
}

}  // namespace

int main() {
    using namespace dusk::psp::room;
    std::vector<std::uint8_t> package = make_package();
    PackageView view = {};
    if (validate_room_dptx(
            package.data(), static_cast<std::uint32_t>(package.size()),
            &view) != PackageError::Ok) {
        return 1;
    }
    MaterialPassPlan plan = {};
    if (read_room_material_pass_plan(view, 0, &plan) != PackageError::Ok ||
        plan.fidelity != MaterialPassFidelity::Approximate ||
        plan.reason != MaterialFallbackReason::UnsupportedTexgen ||
        plan.pass_count != 2 || plan.passes[0].texture_id != 0 ||
        !plan.passes[0].depth_write || !plan.passes[0].use_texture ||
        plan.passes[1].texture_id != 1 ||
        plan.passes[1].texture_effect != MaterialTextureEffect::Replace ||
        plan.passes[1].blend != MaterialBlendPolicy::Multiply ||
        plan.passes[1].depth_write || !plan.passes[1].use_texture) {
        return 2;
    }
    package[560 + 33] |= 1u;
    refresh_crc(package);
    if (validate_room_dptx(
            package.data(), static_cast<std::uint32_t>(package.size()),
            &view) != PackageError::Layout) {
        return 3;
    }
    package = make_package();
    put_u16(package, 560 + 8, 1);
    refresh_crc(package);
    if (validate_room_dptx(
            package.data(), static_cast<std::uint32_t>(package.size()),
            &view) != PackageError::Texture) {
        return 4;
    }
    std::puts(
        "MATERIAL_PASS_PLAN_HOST_OK plans=1 passes=2 negative_cases=2");
    return 0;
}
