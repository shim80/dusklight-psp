#include "dusk/psp/movebg_runtime.hpp"
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

bool validate_part(
    const char* model_path,
    const char* texture_path,
    const char* collision_path,
    std::uint32_t* triangles) {
    std::vector<std::uint8_t> model_bytes;
    std::vector<std::uint8_t> texture_bytes;
    std::vector<std::uint8_t> collision_bytes;
    if (!read_file(model_path, &model_bytes) ||
        !read_file(texture_path, &texture_bytes) ||
        !read_file(collision_path, &collision_bytes)) {
        return false;
    }
    dusk::psp::room::PackageView model = {};
    dusk::psp::room::PackageView textures = {};
    dusk::psp::room::PackageView collision = {};
    if (dusk::psp::room::validate_dprm(
            model_bytes.data(),
            static_cast<std::uint32_t>(model_bytes.size()),
            &model) != dusk::psp::room::PackageError::Ok ||
        dusk::psp::room::validate_room_dptx(
            texture_bytes.data(),
            static_cast<std::uint32_t>(texture_bytes.size()),
            &textures) != dusk::psp::room::PackageError::Ok ||
        dusk::psp::room::validate_dpcl(
            collision_bytes.data(),
            static_cast<std::uint32_t>(collision_bytes.size()),
            &collision) != dusk::psp::room::PackageError::Ok) {
        return false;
    }
    *triangles = dusk::psp::room::read_u32(collision.bytes + 32);
    if (*triangles == 0 ||
        dusk::psp::room::read_u32(model.bytes + 32) == 0 ||
        dusk::psp::room::read_u32(textures.bytes + 16) == 0) {
        return false;
    }

    dusk::psp::movebg::PspMoveBgWorld world;
    world.initialize();
    auto matrix = dusk::psp::movebg::identity_matrix();
    dusk::psp::movebg::Handle handle = {};
    if (!world.create(
            collision.bytes, collision.size, matrix, &handle)) {
        return false;
    }
    matrix.value[1][3] = 50.0f;
    if (!world.update(handle, matrix) ||
        world.matrix(handle) == nullptr ||
        world.matrix(handle)->value[1][3] != 50.0f ||
        !world.destroy(handle) ||
        world.valid(handle) ||
        world.metrics.dynamic_collision_frame_lag != 0) {
        return false;
    }
    world.shutdown();
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    std::uint32_t base_triangles = 0;
    std::uint32_t top_triangles = 0;
    if (argc != 7 ||
        !validate_part(
            argv[1], argv[2], argv[3], &base_triangles) ||
        !validate_part(
            argv[4], argv[5], argv[6], &top_triangles)) {
        std::fputs("SPINNER_SWITCH_ASSET_HOST_FAILED\n", stderr);
        return 1;
    }
    std::printf(
        "SPINNER_SWITCH_ASSET_HOST_OK source=swspin "
        "archive=P_Sswitch models=4,5 collisions=9,8 "
        "triangles=%u,%u formats=DPRM,DPTX,DPCL "
        "frame_lag=0\n",
        base_triangles, top_triangles);
    return 0;
}
