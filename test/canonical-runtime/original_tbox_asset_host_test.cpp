#include "dusk/psp/playable_package.hpp"
#include "dusk/psp/room_package.hpp"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>

namespace {

bool read_file(const char* path, std::vector<std::uint8_t>* bytes) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input || input.tellg() <= 0) {
        return false;
    }
    bytes->resize(static_cast<std::size_t>(input.tellg()));
    input.seekg(0);
    return static_cast<bool>(input.read(
        reinterpret_cast<char*>(bytes->data()),
        static_cast<std::streamsize>(bytes->size())));
}

bool run_test(int argc, char** argv) {
    if (argc != 6) {
        return false;
    }
    std::vector<std::uint8_t> model_bytes;
    std::vector<std::uint8_t> texture_bytes;
    std::vector<std::uint8_t> animation_bytes;
    std::vector<std::uint8_t> closed_bytes;
    std::vector<std::uint8_t> open_bytes;
    if (!read_file(argv[1], &model_bytes) ||
        !read_file(argv[2], &texture_bytes) ||
        !read_file(argv[3], &animation_bytes) ||
        !read_file(argv[4], &closed_bytes) ||
        !read_file(argv[5], &open_bytes)) {
        return false;
    }

    dusk::psp::room::PackageView model = {};
    dusk::psp::room::PackageView textures = {};
    dusk::psp::room::PackageView closed = {};
    dusk::psp::room::PackageView open = {};
    dusk::psp::playable::PackageView animation = {};
    if (dusk::psp::room::validate_dprm(
            model_bytes.data(),
            static_cast<std::uint32_t>(model_bytes.size()),
            &model) != dusk::psp::room::PackageError::Ok ||
        dusk::psp::room::validate_room_dptx(
            texture_bytes.data(),
            static_cast<std::uint32_t>(texture_bytes.size()),
            &textures) != dusk::psp::room::PackageError::Ok ||
        dusk::psp::playable::validate_dpan(
            animation_bytes.data(),
            static_cast<std::uint32_t>(animation_bytes.size()),
            &animation) != dusk::psp::playable::PackageError::Ok ||
        dusk::psp::room::validate_dpcl(
            closed_bytes.data(),
            static_cast<std::uint32_t>(closed_bytes.size()),
            &closed) != dusk::psp::room::PackageError::Ok ||
        dusk::psp::room::validate_dpcl(
            open_bytes.data(),
            static_cast<std::uint32_t>(open_bytes.size()),
            &open) != dusk::psp::room::PackageError::Ok) {
        return false;
    }

    const std::uint32_t clips =
        dusk::psp::playable::read_u32(animation.bytes + 16);
    const std::uint32_t joints =
        dusk::psp::playable::read_u32(animation.bytes + 20);
    const std::uint32_t table =
        dusk::psp::playable::read_u32(animation.bytes + 32);
    const std::uint32_t animation_id =
        dusk::psp::playable::read_u32(animation.bytes + table);
    const std::uint32_t closed_triangles =
        dusk::psp::room::read_u32(closed.bytes + 20);
    const std::uint32_t open_triangles =
        dusk::psp::room::read_u32(open.bytes + 20);
    return clips == 1 && joints == 3 && animation_id == 8 &&
           dusk::psp::room::read_u32(model.bytes + 32) != 0 &&
           dusk::psp::room::read_u32(textures.bytes + 16) != 0 &&
           closed_triangles != 0 && open_triangles != 0 &&
           closed_triangles != open_triangles;
}

}  // namespace

int main(int argc, char** argv) {
    if (!run_test(argc, argv)) {
        std::fputs("ORIGINAL_TBOX_ASSET_HOST_FAILED\n", stderr);
        return 1;
    }
    std::puts(
        "ORIGINAL_TBOX_ASSET_HOST_OK archive=Dalways "
        "model=13 animation=8 collisions=27,28 joints=3 "
        "formats=DPRM,DPTX,DPAN,DPCL");
    return 0;
}
