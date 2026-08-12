#include "dusk/psp/movebg_runtime.hpp"
#include "dusk/psp/room_package.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>

namespace {

using dusk::psp::room::PackageError;
using dusk::psp::room::PackageView;
using dusk::psp::room::Vec3;

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

Vec3 add(const Vec3& left, const Vec3& right) {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

Vec3 scale(const Vec3& value, float amount) {
    return {value.x * amount, value.y * amount, value.z * amount};
}

bool close(float left, float right) {
    return std::fabs(left - right) < 0.05f;
}

bool close(const Vec3& left, const Vec3& right) {
    return close(left.x, right.x) &&
           close(left.y, right.y) &&
           close(left.z, right.z);
}

Vec3 vertex_at(
    const PackageView& collision, std::uint16_t vertex_index) {
    const std::uint32_t offset =
        dusk::psp::room::read_u32(collision.bytes + 68);
    const std::uint32_t stride =
        dusk::psp::room::read_u32(collision.bytes + 72);
    const std::uint8_t* vertex =
        collision.bytes + offset + vertex_index * stride;
    return {
        dusk::psp::room::read_f32(vertex),
        dusk::psp::room::read_f32(vertex + 4),
        dusk::psp::room::read_f32(vertex + 8),
    };
}

bool run_test(
    const char* model_path,
    const char* texture_path,
    const char* collision_path) {
    std::vector<std::uint8_t> model_bytes;
    std::vector<std::uint8_t> texture_bytes;
    std::vector<std::uint8_t> collision_bytes;
    if (!read_file(model_path, &model_bytes) ||
        !read_file(texture_path, &texture_bytes) ||
        !read_file(collision_path, &collision_bytes)) {
        return false;
    }

    PackageView model = {};
    PackageView textures = {};
    PackageView collision = {};
    if (dusk::psp::room::validate_dprm(
            model_bytes.data(),
            static_cast<std::uint32_t>(model_bytes.size()),
            &model) != PackageError::Ok ||
        dusk::psp::room::validate_room_dptx(
            texture_bytes.data(),
            static_cast<std::uint32_t>(texture_bytes.size()),
            &textures) != PackageError::Ok ||
        dusk::psp::room::validate_dpcl(
            collision_bytes.data(),
            static_cast<std::uint32_t>(collision_bytes.size()),
            &collision) != PackageError::Ok) {
        return false;
    }

    const std::uint32_t triangle_offset =
        dusk::psp::room::read_u32(collision.bytes + 76);
    const std::uint32_t triangle_stride =
        dusk::psp::room::read_u32(collision.bytes + 80);
    const std::uint8_t* triangle = collision.bytes + triangle_offset;
    const Vec3 first = vertex_at(
        collision, dusk::psp::room::read_u16(triangle));
    const Vec3 second = vertex_at(
        collision, dusk::psp::room::read_u16(triangle + 2));
    const Vec3 third = vertex_at(
        collision, dusk::psp::room::read_u16(triangle + 4));
    const Vec3 centroid = scale(
        add(add(first, second), third), 1.0f / 3.0f);
    const Vec3 normal = {
        dusk::psp::room::read_f32(triangle + 8),
        dusk::psp::room::read_f32(triangle + 12),
        dusk::psp::room::read_f32(triangle + 16),
    };
    if (triangle_stride != 32 ||
        !std::isfinite(normal.x) ||
        !std::isfinite(normal.y) ||
        !std::isfinite(normal.z)) {
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

    constexpr float kTraceDistance = 50.0f;
    const Vec3 start = add(centroid, scale(normal, kTraceDistance));
    const Vec3 end = add(centroid, scale(normal, -kTraceDistance));
    const auto initial = world.trace_camera(handle, start, end);
    if (!initial.hit || !close(initial.position, centroid)) {
        return false;
    }

    const Vec3 translation = {17.0f, 43.0f, -11.0f};
    matrix.value[0][3] = translation.x;
    matrix.value[1][3] = translation.y;
    matrix.value[2][3] = translation.z;
    if (!world.update(handle, matrix)) {
        return false;
    }
    const auto moved = world.trace_camera(
        handle, add(start, translation), add(end, translation));
    Vec3 carried = {};
    const auto* committed = world.matrix(handle);
    if (!moved.hit ||
        !close(moved.position, add(initial.position, translation)) ||
        !world.carry_point(handle, centroid, &carried) ||
        !close(carried, add(centroid, translation)) ||
        committed == nullptr ||
        !close(committed->value[0][3], translation.x) ||
        !close(committed->value[1][3], translation.y) ||
        !close(committed->value[2][3], translation.z) ||
        !world.destroy(handle) ||
        world.valid(handle) ||
        world.update(handle, matrix) ||
        world.metrics.creates != 1 ||
        world.metrics.updates != 1 ||
        world.metrics.commits != 2 ||
        world.metrics.deletes != 1 ||
        world.metrics.camera_queries != 2 ||
        world.metrics.carry_updates != 1 ||
        world.metrics.dynamic_collision_frame_lag != 0) {
        return false;
    }
    world.shutdown();
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 4 || !run_test(argv[1], argv[2], argv[3])) {
        std::fputs("ORIGINAL_DOOR_ASSET_HOST_FAILED\n", stderr);
        return 1;
    }
    std::printf(
        "ORIGINAL_DOOR_ASSET_HOST_OK source=L4Pgate "
        "archive=L4R02Gate resources=4,7 formats=DPRM,DPTX,DPCL "
        "dynamic_translation=true frame_lag=0\n");
    return 0;
}
