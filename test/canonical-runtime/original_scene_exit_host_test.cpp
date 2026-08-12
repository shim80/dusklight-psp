#include "dusk/psp/original_scene_exit_bridge.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

namespace compat = dusk::psp::compat;
namespace process = dusk::psp::process;
namespace room = dusk::psp::room;

namespace {

struct Requests {
    std::uint32_t calls;
    std::uint8_t exit_index;
    std::uint8_t path_id;
    bool jump;
};

bool request(
    void* user, std::uint8_t exit_index,
    std::uint8_t path_id, bool jump, const float*) {
    auto* requests = static_cast<Requests*>(user);
    ++requests->calls;
    requests->exit_index = exit_index;
    requests->path_id = path_id;
    requests->jump = jump;
    return requests->calls == 1;
}

bool no_switch(void*, std::int8_t, std::uint8_t) {
    return false;
}

void on_switch(void*, std::int8_t, std::uint8_t) {}

bool no_event(void*, std::uint16_t) {
    return false;
}

bool close(float left, float right) {
    return std::fabs(left - right) < 0.01f;
}

std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream input(path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

bool exercise(const char* path, std::int8_t room_number) {
    const std::vector<std::uint8_t> bytes = read_file(path);
    room::PackageView scene = {};
    if (room::validate_dpsc(
            bytes.data(), static_cast<std::uint32_t>(bytes.size()),
            &scene) != room::PackageError::Ok) {
        return false;
    }
    room::SceneTriggerV3 trigger = {};
    if (room::read_dpsc_trigger_v3(
            scene, 0, &trigger) != room::PackageError::Ok) {
        return false;
    }
    process::PspProcessManager manager;
    manager.initialize();
    if (!compat::register_original_scene_exit_profile(&manager) ||
        manager.profile_count() != 1 ||
        compat::register_original_scene_exit_profile(&manager)) {
        return false;
    }
    process::ProcessHandle handle = {};
    if (!compat::create_original_scene_exit(
            &manager, scene, room_number, &handle)) {
        return false;
    }
    compat::SceneExitSnapshot snapshot = {};
    if (!compat::inspect_original_scene_exit(
            manager, handle, &snapshot) ||
        snapshot.parameters != trigger.parameters ||
        snapshot.room != room_number ||
        !close(snapshot.dimensions[0], trigger.dimensions[0]) ||
        !close(snapshot.dimensions[1], trigger.dimensions[1]) ||
        !close(snapshot.dimensions[2], trigger.dimensions[2])) {
        return false;
    }
    Requests requests = {};
    compat::bind_scene_exit_facade({
        &requests, request, no_switch, on_switch, on_switch, no_event,
        nullptr});
    const float outside[3] = {
        trigger.position[0] + trigger.dimensions[0] + 10.0f,
        trigger.position[1] + 1.0f,
        trigger.position[2],
    };
    compat::set_scene_exit_player_position(outside);
    if (!manager.execute_all() || requests.calls != 0) {
        return false;
    }
    const float inside[3] = {
        trigger.position[0],
        trigger.position[1] + 1.0f,
        trigger.position[2],
    };
    compat::set_scene_exit_player_position(inside);
    if (!manager.execute_all() || requests.calls != 1 ||
        requests.exit_index != trigger.exit_index ||
        requests.path_id !=
            static_cast<std::uint8_t>(
                (trigger.parameters >> 16) & 0xffu) ||
        !manager.draw_all()) {
        return false;
    }
    compat::set_scene_exit_paused(true);
    if (!manager.execute_all() || requests.calls != 1) {
        return false;
    }
    compat::set_scene_exit_paused(false);
    manager.destroy_room(room_number);
    const bool lifecycle =
        manager.active_count() == 0 &&
        !manager.handle_valid(handle) &&
        !manager.destroy(handle) &&
        manager.metrics.create_calls == 1 &&
        manager.metrics.execute_calls == 3 &&
        manager.metrics.draw_calls == 1 &&
        manager.metrics.is_delete_calls == 1 &&
        manager.metrics.delete_calls == 1 &&
        manager.metrics.stale_handles == 1;
    compat::unbind_scene_exit_facade();
    return lifecycle;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3 || !compat::original_profile_valid() ||
        !exercise(argv[1], 9) || !exercise(argv[2], 2) ||
        compat::specialized_psp_trigger_logic_calls() != 0) {
        return 1;
    }
    std::puts(
        "ORIGINAL_SCENE_EXIT_HOST_OK process_id=0x030C "
        "source=dusklight-main/src/d/actor/d_a_scene_exit.cpp "
        "negative_cases=4");
    return 0;
}
