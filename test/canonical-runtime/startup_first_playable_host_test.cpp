#include "dusk/psp/actor_runtime.hpp"
#include "dusk/psp/real_room_runtime.hpp"
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

bool near(float value, float expected) {
    return std::fabs(value - expected) < 0.1f;
}

}  // namespace

int main(int argc, char** argv) {
    using namespace dusk::psp;
    if (argc != 5) {
        return 2;
    }
    const auto model_bytes = load(argv[1]);
    const auto texture_bytes = load(argv[2]);
    const auto collision_bytes = load(argv[3]);
    const auto scene_bytes = load(argv[4]);
    room::PackageView model = {};
    room::PackageView textures = {};
    room::PackageView collision = {};
    room::PackageView scene = {};
    const auto model_error = room::validate_dprm(
        model_bytes.data(), model_bytes.size(), &model);
    const auto texture_error = room::validate_room_dptx(
        texture_bytes.data(), texture_bytes.size(), &textures);
    const auto collision_error = room::validate_dpcl(
        collision_bytes.data(), collision_bytes.size(), &collision);
    const auto scene_error = room::validate_dpsc(
        scene_bytes.data(), scene_bytes.size(), &scene);
    if (model_error != room::PackageError::Ok ||
        texture_error != room::PackageError::Ok ||
        collision_error != room::PackageError::Ok ||
        scene_error != room::PackageError::Ok ||
        room::read_u32(scene_bytes.data() + 136) != 599) {
        std::fprintf(
            stderr, "F_SP108 validation failed model=%s textures=%s "
            "collision=%s scene=%s actors=%u\n",
            room::package_error_name(model_error),
            room::package_error_name(texture_error),
            room::package_error_name(collision_error),
            room::package_error_name(scene_error),
            static_cast<unsigned int>(
                room::read_u32(scene_bytes.data() + 136)));
        return 3;
    }
    room::SceneSpawnV3 spawn = {};
    room::RealRoomRuntime runtime = {};
    actor::ActorSystem actors = {};
    if (room::find_dpsc_spawn_v3(scene, 21, &spawn) !=
            room::PackageError::Ok ||
        !room::initialize_real_room_runtime(
            &runtime, model, textures, collision, scene) ||
        !room::spawn_real_room(&runtime, 21) ||
        !room::real_room_state_consistent(runtime) ||
        actor::initialize_actor_system(&actors, scene) !=
            actor::Error::Ok ||
        actors.essential_source_actor_count != 9 ||
        actors.active_count != 9 ||
        actors.create_calls != 9 ||
        !actor::actor_system_consistent(actors) ||
        !near(runtime.state.position.x, -16922.4f) ||
        std::fabs(runtime.state.position.y - spawn.position[1]) > 0.00001f ||
        std::fabs(runtime.state.position.y - spawn.floor_height) < 0.0001f ||
        !near(runtime.state.position.z, -4467.3f)) {
        return 4;
    }
    playable::Input idle = {};
    room::update_real_room(&runtime, idle, 1.0f / 30.0f);
    if (std::fabs(runtime.state.position.y - spawn.floor_height) > 0.0001f) {
        return 5;
    }
    std::puts(
        "STARTUP_FIRST_PLAYABLE_HOST_OK stage=F_SP108 room=1 "
        "start=21 source_actors=599 instantiated_actors=9");
    return 0;
}
