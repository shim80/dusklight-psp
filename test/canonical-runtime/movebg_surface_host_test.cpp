#include "dusk/psp/movebg_runtime.hpp"

#include <cmath>
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

bool close(float a, float b) {
    return std::fabs(a - b) < 0.01f;
}

bool run_test(const char* path) {
    std::vector<std::uint8_t> bytes;
    if (!read_file(path, &bytes)) {
        return false;
    }
    dusk::psp::movebg::PspMoveBgWorld world;
    world.initialize();
    auto matrix = dusk::psp::movebg::identity_matrix();
    dusk::psp::movebg::Handle handle = {};
    if (!world.create(
            bytes.data(), static_cast<std::uint32_t>(bytes.size()),
            matrix, &handle)) {
        return false;
    }
    const dusk::psp::room::Vec3 query = {0.0f, 500.0f, -4575.0f};
    const auto initial = world.find_floor(handle, query, 100.0f, 0.6f);
    if (!initial.hit) {
        return false;
    }
    matrix.value[0][3] = 20.0f;
    matrix.value[1][3] = 100.0f;
    if (!world.update(handle, matrix)) {
        return false;
    }
    const auto moved = world.find_floor(
        handle, {20.0f, 600.0f, -4575.0f}, 100.0f, 0.6f);
    dusk::psp::room::Vec3 carried = {};
    if (!moved.hit ||
        !close(moved.height, initial.height + 100.0f) ||
        !world.carry_point(
            handle, {0.0f, initial.height, -4575.0f}, &carried) ||
        !close(carried.x, 20.0f) ||
        !close(carried.y, initial.height + 100.0f)) {
        return false;
    }
    const auto camera = world.trace_camera(
        handle, {20.0f, 700.0f, -4575.0f},
        {20.0f, 400.0f, -4575.0f});
    if (!camera.hit || !world.destroy(handle) ||
        world.valid(handle) ||
        world.update(handle, matrix) ||
        world.metrics.dynamic_collision_frame_lag != 0) {
        return false;
    }
    world.shutdown();
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2 || !run_test(argv[1])) {
        std::fputs("MOVEBG_SURFACE_HOST_FAILED\n", stderr);
        return 1;
    }
    std::printf(
        "MOVEBG_SURFACE_HOST_OK generation=true floor=true "
        "camera=true carry=true frame_lag=0\n");
    return 0;
}
