#include "dusk/psp/room_package.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

namespace {

std::vector<std::uint8_t> load(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()};
}

bool model_valid(const char* path) {
    const std::vector<std::uint8_t> bytes = load(path);
    dusk::psp::room::PackageView view = {};
    return !bytes.empty() &&
           dusk::psp::room::validate_dprm(
               bytes.data(), static_cast<std::uint32_t>(bytes.size()),
               &view) == dusk::psp::room::PackageError::Ok;
}

bool textures_valid(const char* path) {
    const std::vector<std::uint8_t> bytes = load(path);
    dusk::psp::room::PackageView view = {};
    return !bytes.empty() &&
           dusk::psp::room::validate_room_dptx(
               bytes.data(), static_cast<std::uint32_t>(bytes.size()),
               &view) == dusk::psp::room::PackageError::Ok;
}

bool title_model_contract(const char* path) {
    const std::vector<std::uint8_t> bytes = load(path);
    if (bytes.size() < 384 ||
        dusk::psp::room::read_u32(bytes.data() + 20) != 24 ||
        dusk::psp::room::read_u32(bytes.data() + 32) != 6) {
        return false;
    }
    constexpr float kExpectedBounds[6] = {
        -228.0f, -168.0f, 0.0f, 228.0f, 168.0f, 0.0f};
    for (std::uint32_t index = 0; index < 6; ++index) {
        if (std::fabs(
                dusk::psp::room::read_f32(
                    bytes.data() + 48 + index * 4) -
                kExpectedBounds[index]) > 0.001f) {
            return false;
        }
    }
    const std::uint32_t section_table =
        dusk::psp::room::read_u32(bytes.data() + 72);
    const std::uint32_t submeshes = dusk::psp::room::read_u32(
        bytes.data() + section_table + 64 + 4);
    if (submeshes > bytes.size() ||
        bytes.size() - submeshes < 6 * 48) {
        return false;
    }
    for (std::uint32_t index = 0; index < 6; ++index) {
        const std::uint8_t* item = bytes.data() + submeshes + index * 48;
        if (item[12] != 2 ||
            dusk::psp::room::read_u16(item + 14) != index) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 5) {
        std::fputs("startup title asset arguments invalid\n", stderr);
        return 1;
    }
    const bool room_model = model_valid(argv[1]);
    const bool room_textures = textures_valid(argv[2]);
    const bool title_model = model_valid(argv[3]);
    const bool title_textures = textures_valid(argv[4]);
    const bool title_local_geometry = title_model_contract(argv[3]);
    if (!room_model || !room_textures ||
        !title_model || !title_textures || !title_local_geometry) {
        std::fprintf(
            stderr,
            "startup title asset validation failed "
            "room_model=%s room_textures=%s "
            "title_model=%s title_textures=%s\n",
            room_model ? "true" : "false",
            room_textures ? "true" : "false",
            title_model ? "true" : "false",
            title_textures ? "true" : "false");
        std::fprintf(
            stderr, "title_local_geometry=%s\n",
            title_local_geometry ? "true" : "false");
        return 1;
    }
    std::puts(
        "STARTUP_TITLE_ASSET_HOST_OK room=DPRM,DPTX "
        "title=DPRM,DPTX local_vertices=24 blend_submeshes=6");
    return 0;
}
