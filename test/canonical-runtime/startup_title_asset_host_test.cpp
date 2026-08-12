#include "dusk/psp/room_package.hpp"

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
    if (!room_model || !room_textures ||
        !title_model || !title_textures) {
        std::fprintf(
            stderr,
            "startup title asset validation failed "
            "room_model=%s room_textures=%s "
            "title_model=%s title_textures=%s\n",
            room_model ? "true" : "false",
            room_textures ? "true" : "false",
            title_model ? "true" : "false",
            title_textures ? "true" : "false");
        return 1;
    }
    std::puts(
        "STARTUP_TITLE_ASSET_HOST_OK room=DPRM,DPTX "
        "title=DPRM,DPTX");
    return 0;
}
