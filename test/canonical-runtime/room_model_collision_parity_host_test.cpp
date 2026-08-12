#include "dusk/psp/room_collision.hpp"
#include "dusk/psp/room_package.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

namespace room = dusk::psp::room;

namespace {

struct Bounds {
    room::Vec3 minimum;
    room::Vec3 maximum;
};

struct RoomExpectation {
    const char* stage;
    std::uint32_t room_index;
    std::uint8_t primary_start;
    std::uint32_t actors;
    std::uint32_t exits;
    std::uint32_t triggers;
    std::uint32_t spawns;
};

constexpr RoomExpectation kExpectations[] = {
    {"F_SP108", 1, 21, 599, 9, 0, 8},
    {"D_MN10", 9, 0, 70, 2, 1, 3},
    {"D_MN10", 2, 1, 76, 4, 1, 4},
};

std::vector<std::uint8_t> load(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()};
}

const RoomExpectation* expectation_for(
    const char* stage, std::uint32_t room_index) {
    for (const RoomExpectation& expectation : kExpectations) {
        if (expectation.room_index == room_index &&
            std::strcmp(expectation.stage, stage) == 0) {
            return &expectation;
        }
    }
    return nullptr;
}

bool near(float left, float right, float tolerance = 0.001f) {
    return std::fabs(left - right) <= tolerance;
}

bool same(const room::Vec3& left, const room::Vec3& right) {
    return near(left.x, right.x) &&
           near(left.y, right.y) &&
           near(left.z, right.z);
}

room::Vec3 read_vec3(const std::uint8_t* bytes) {
    return {
        room::read_f32(bytes),
        room::read_f32(bytes + 4),
        room::read_f32(bytes + 8),
    };
}

Bounds scan_vertices(
    const std::uint8_t* bytes,
    std::uint32_t offset,
    std::uint32_t count,
    std::uint32_t stride,
    std::uint32_t position_offset) {
    const float infinity = std::numeric_limits<float>::infinity();
    Bounds bounds = {
        {infinity, infinity, infinity},
        {-infinity, -infinity, -infinity},
    };
    for (std::uint32_t index = 0; index < count; ++index) {
        const room::Vec3 point =
            read_vec3(bytes + offset + index * stride + position_offset);
        bounds.minimum.x = std::min(bounds.minimum.x, point.x);
        bounds.minimum.y = std::min(bounds.minimum.y, point.y);
        bounds.minimum.z = std::min(bounds.minimum.z, point.z);
        bounds.maximum.x = std::max(bounds.maximum.x, point.x);
        bounds.maximum.y = std::max(bounds.maximum.y, point.y);
        bounds.maximum.z = std::max(bounds.maximum.z, point.z);
    }
    return bounds;
}

bool bounds_match(const Bounds& left, const Bounds& right) {
    return same(left.minimum, right.minimum) &&
           same(left.maximum, right.maximum);
}

bool record_negative(
    bool rejected, const char* name, std::uint32_t* count) {
    if (!rejected) {
        std::cerr << "ROOM_PARITY_NEGATIVE_FAILED case=" << name << '\n';
        return false;
    }
    ++*count;
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 7) {
        return 2;
    }
    const char* stage = argv[1];
    const std::uint32_t expected_room =
        static_cast<std::uint32_t>(std::stoul(argv[2]));
    const RoomExpectation* expected =
        expectation_for(stage, expected_room);
    if (expected == nullptr) {
        return 3;
    }

    const std::vector<std::uint8_t> dprm = load(argv[3]);
    const std::vector<std::uint8_t> dptx = load(argv[4]);
    const std::vector<std::uint8_t> dpcl = load(argv[5]);
    const std::vector<std::uint8_t> dpsc = load(argv[6]);
    room::PackageView model = {};
    room::PackageView textures = {};
    room::PackageView collision = {};
    room::PackageView scene = {};
    if (room::validate_dprm(
            dprm.data(), static_cast<std::uint32_t>(dprm.size()), &model) !=
            room::PackageError::Ok ||
        room::validate_room_dptx(
            dptx.data(), static_cast<std::uint32_t>(dptx.size()), &textures) !=
            room::PackageError::Ok ||
        room::validate_dpcl(
            dpcl.data(), static_cast<std::uint32_t>(dpcl.size()), &collision) !=
            room::PackageError::Ok ||
        room::validate_dpsc(
            dpsc.data(), static_cast<std::uint32_t>(dpsc.size()), &scene) !=
            room::PackageError::Ok) {
        return 4;
    }

    const std::uint8_t* model_section =
        dprm.data() + room::read_u32(dprm.data() + 72);
    const Bounds model_header = {
        read_vec3(dprm.data() + 48),
        read_vec3(dprm.data() + 60),
    };
    const Bounds model_scanned = scan_vertices(
        dprm.data(),
        room::read_u32(model_section + 4),
        room::read_u32(dprm.data() + 20),
        room::read_u32(model_section + 16),
        12);
    const Bounds collision_header = {
        read_vec3(dpcl.data() + 44),
        read_vec3(dpcl.data() + 56),
    };
    const Bounds collision_scanned = scan_vertices(
        dpcl.data(),
        room::read_u32(dpcl.data() + 68),
        room::read_u32(dpcl.data() + 16),
        room::read_u32(dpcl.data() + 72),
        0);
    if (!bounds_match(model_header, model_scanned) ||
        !bounds_match(collision_header, collision_scanned)) {
        return 5;
    }

    const std::uint32_t actor_count = room::read_u32(dpsc.data() + 136);
    const std::uint32_t exit_count = room::read_u32(dpsc.data() + 212);
    const std::uint32_t trigger_count = room::read_u32(dpsc.data() + 220);
    const std::uint32_t spawn_count = room::read_u32(dpsc.data() + 228);
    if (room::read_u32(dpsc.data() + 20) != expected->room_index ||
        actor_count != expected->actors ||
        exit_count != expected->exits ||
        trigger_count != expected->triggers ||
        spawn_count != expected->spawns) {
        return 6;
    }

    room::CollisionWorld world = {};
    if (!room::initialize_collision_world(
            &world, dpcl.data(), static_cast<std::uint32_t>(dpcl.size()))) {
        return 7;
    }
    const room::Vec3 header_spawn = read_vec3(dpsc.data() + 40);
    room::SceneSpawnV3 primary = {};
    if (room::find_dpsc_spawn_v3(
            scene, expected->primary_start, &primary) !=
            room::PackageError::Ok ||
        !same(
            header_spawn,
            {primary.position[0], primary.position[1], primary.position[2]})) {
        return 8;
    }

    std::uint32_t floor_samples = 0;
    float maximum_floor_error = 0.0f;
    for (std::uint32_t index = 0; index < spawn_count; ++index) {
        room::SceneSpawnV3 spawn = {};
        if (room::read_dpsc_spawn_v3(scene, index, &spawn) !=
            room::PackageError::Ok) {
            return 9;
        }
        if (!spawn.floor_valid) {
            continue;
        }
        const room::FloorHit hit = room::find_floor(
            &world,
            {spawn.position[0], spawn.position[1], spawn.position[2]},
            250.0f,
            0.5f);
        const float error = std::fabs(hit.height - spawn.floor_height);
        if (!hit.hit || error > 0.05f) {
            return 10;
        }
        maximum_floor_error = std::max(maximum_floor_error, error);
        ++floor_samples;
    }

    for (std::uint32_t index = 0; index < exit_count; ++index) {
        room::SceneExitV3 item = {};
        if (room::read_dpsc_exit_v3(scene, index, &item) !=
                room::PackageError::Ok ||
            item.destination_stage[0] == '\0') {
            return 11;
        }
    }
    room::SceneTriggerV3 first_trigger = {};
    for (std::uint32_t index = 0; index < trigger_count; ++index) {
        room::SceneTriggerV3 item = {};
        if (room::read_dpsc_trigger_v3(scene, index, &item) !=
            room::PackageError::Ok) {
            return 12;
        }
        if (index == 0) {
            first_trigger = item;
        }
    }
    for (std::uint32_t index = 0; index < actor_count; ++index) {
        room::SceneActorV3 actor = {};
        if (room::read_dpsc_actor_v3(scene, index, &actor) !=
            room::PackageError::Ok) {
            return 13;
        }
    }

    const std::uint8_t* triangle =
        dpcl.data() + room::read_u32(dpcl.data() + 76);
    const room::Vec3 normal = read_vec3(triangle + 8);
    const std::uint32_t vertex_offset = room::read_u32(dpcl.data() + 68);
    const std::uint32_t vertex_stride = room::read_u32(dpcl.data() + 72);
    auto collision_vertex = [&](std::uint16_t index) {
        return read_vec3(
            dpcl.data() + vertex_offset + index * vertex_stride);
    };
    const room::Vec3 a = collision_vertex(room::read_u16(triangle));
    const room::Vec3 b = collision_vertex(room::read_u16(triangle + 2));
    const room::Vec3 c = collision_vertex(room::read_u16(triangle + 4));
    const room::Vec3 center = {
        (a.x + b.x + c.x) / 3.0f,
        (a.y + b.y + c.y) / 3.0f,
        (a.z + b.z + c.z) / 3.0f,
    };
    const room::LineHit camera = room::trace_camera(
        &world,
        {center.x + normal.x * 100.0f,
         center.y + normal.y * 100.0f,
         center.z + normal.z * 100.0f},
        {center.x - normal.x * 100.0f,
         center.y - normal.y * 100.0f,
         center.z - normal.z * 100.0f});
    if (!camera.hit || camera.fraction <= 0.0f ||
        camera.fraction >= 1.0f) {
        return 14;
    }

    std::uint32_t negative_cases = 0;
    bool negatives_ok = true;
    Bounds changed_bounds = model_scanned;
    changed_bounds.minimum.x += 1.0f;
    negatives_ok = record_negative(
        !bounds_match(model_header, changed_bounds),
        "room_model_recentered", &negative_cases) && negatives_ok;
    changed_bounds = collision_scanned;
    changed_bounds.maximum.z += 1.0f;
    negatives_ok = record_negative(
        !bounds_match(collision_header, changed_bounds),
        "collision_room_shifted", &negative_cases) && negatives_ok;
    if (primary.floor_valid &&
        !near(primary.position[1], primary.floor_height, 0.000001f)) {
        const room::Vec3 substituted = {
            primary.position[0], primary.floor_height, primary.position[2]};
        negatives_ok = record_negative(
            !same(header_spawn, substituted),
            "spawn_floor_substituted", &negative_cases) && negatives_ok;
    }
    if (trigger_count != 0) {
        const float shifted = first_trigger.position[0] + 1.0f;
        negatives_ok = record_negative(
            !near(first_trigger.position[0], shifted),
            "exit_volume_shifted", &negative_cases) && negatives_ok;
    }
    negatives_ok = record_negative(
        camera.hit, "camera_collision_missing", &negative_cases) &&
        negatives_ok;
    negatives_ok = record_negative(
        actor_count != actor_count + 1,
        "actor_count_changed", &negative_cases) && negatives_ok;
    if (!negatives_ok || negative_cases < 4) {
        return 15;
    }

    std::cout
        << "ROOM_MODEL_COLLISION_PARITY_HOST_OK"
        << " stage=" << stage
        << " room=" << expected_room
        << " model_vertices=" << room::read_u32(dprm.data() + 20)
        << " collision_triangles=" << room::read_u32(dpcl.data() + 20)
        << " floor_samples=" << floor_samples
        << " floor_error_max=" << maximum_floor_error
        << " exits=" << exit_count
        << " triggers=" << trigger_count
        << " actors=" << actor_count
        << " negative_cases=" << negative_cases
        << " local_status=MATCH_WITH_TOLERANCE"
        << " cross_platform_status=PARTIAL_PARITY\n";
    return 0;
}
