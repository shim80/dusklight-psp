#include "dusk/psp/room_package.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

void put_u16(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xffu);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put_u32(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(
            (value >> (index * 8)) & 0xffu);
    }
}

std::vector<std::uint8_t> make_package() {
    constexpr std::uint32_t kTextureTable = 128;
    constexpr std::uint32_t kMaterialTable = 176;
    constexpr std::uint32_t kPixels = 208;
    constexpr std::uint32_t kPeTable = 336;
    constexpr std::uint32_t kSize = 376;
    std::vector<std::uint8_t> bytes(kSize, 0);
    bytes[0] = 'D'; bytes[1] = 'P'; bytes[2] = 'T'; bytes[3] = 'X';
    put_u16(bytes, 4, 2);
    put_u16(bytes, 6, 128);
    put_u32(bytes, 8, kSize);
    put_u32(bytes, 16, 1);
    put_u32(bytes, 20, 1);
    put_u32(bytes, 24, kTextureTable);
    put_u32(bytes, 28, 48);
    put_u32(bytes, 32, kMaterialTable);
    put_u32(bytes, 36, 32);
    put_u32(bytes, 40, 2);
    put_u32(bytes, 44, 1);
    put_u32(bytes, 48, 128);
    put_u32(bytes, 60, 1);
    put_u32(bytes, 64, 0x31564550u);  // PEV1
    put_u32(bytes, 68, kPeTable);
    put_u32(bytes, 72, 40);
    put_u32(bytes, 76, 1);
    put_u32(bytes, 80, 1);

    put_u16(bytes, kTextureTable + 4, 8);
    put_u16(bytes, kTextureTable + 6, 8);
    put_u16(bytes, kTextureTable + 8, 8);
    put_u16(bytes, kTextureTable + 10, 8);
    bytes[kTextureTable + 12] = 1;
    put_u32(bytes, kTextureTable + 16, kPixels);
    put_u32(bytes, kTextureTable + 20, 128);

    put_u16(bytes, kMaterialTable, 0);
    bytes[kMaterialTable + 2] = 1;
    bytes[kMaterialTable + 3] = 1;
    bytes[kMaterialTable + 4] = 0xff;
    bytes[kMaterialTable + 5] = 0xff;
    bytes[kMaterialTable + 6] = 0xff;
    bytes[kMaterialTable + 7] = 0xff;
    bytes[kMaterialTable + 8] = 0x60;
    bytes[kMaterialTable + 9] = 0x60;
    bytes[kMaterialTable + 10] = 0x60;
    bytes[kMaterialTable + 11] = 0xff;
    bytes[kMaterialTable + 12] = 1;
    bytes[kMaterialTable + 16] = 1;

    put_u16(bytes, kPeTable, 0);
    bytes[kPeTable + 2] = 1;  // AlphaTest
    bytes[kPeTable + 3] = 0;  // OPAQUE draw buffer in source
    bytes[kPeTable + 4] = 1;
    bytes[kPeTable + 5] = 3;
    bytes[kPeTable + 6] = 1;
    bytes[kPeTable + 7] = 2;
    bytes[kPeTable + 8] = 6;
    bytes[kPeTable + 9] = 128;
    bytes[kPeTable + 10] = 0;
    bytes[kPeTable + 11] = 3;
    bytes[kPeTable + 12] = 255;
    bytes[kPeTable + 13] = 0;
    bytes[kPeTable + 14] = 1;
    bytes[kPeTable + 15] = 0;
    bytes[kPeTable + 16] = 3;
    bytes[kPeTable + 17] = 1;
    bytes[kPeTable + 18] = 1;
    bytes[kPeTable + 19] = 1;
    put_u16(bytes, kPeTable + 20, 0);
    for (std::size_t index = 1; index < 8; ++index) {
        put_u16(bytes, kPeTable + 20 + index * 2, 0xffffu);
    }

    put_u32(bytes, 12, dusk::psp::room::package_crc32(
        bytes.data(), static_cast<std::uint32_t>(bytes.size())));
    return bytes;
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
    AlphaMaterialState state = {};
    if (read_room_alpha_material_state(view, 0, &state) != PackageError::Ok ||
        state.material_class != AlphaMaterialClass::AlphaTest ||
        alpha_material_bucket(state.material_class) != 1 ||
        !state.depth_test || !state.depth_write || state.depth_func != 3 ||
        state.alpha_comp0 != 6 || state.alpha_ref0 != 128 ||
        state.alpha_op != 0 || state.alpha_comp1 != 3 ||
        state.alpha_ref1 != 255 || state.cull_mode != 2 ||
        state.texture_count != 1 || !state.texture_identities_complete ||
        state.texture_ids[0] != 0) {
        return 2;
    }
    package[336 + 2] = 0;  // Class says opaque but material bucket remains alpha-test.
    put_u32(package, 12, 0);
    put_u32(package, 12, package_crc32(
        package.data(), static_cast<std::uint32_t>(package.size())));
    if (validate_room_dptx(
            package.data(), static_cast<std::uint32_t>(package.size()),
            &view) != PackageError::Bucket) {
        return 3;
    }
    std::puts("ALPHA_MATERIAL_STATE_HOST_OK records=1 negative_cases=1");
    return 0;
}
