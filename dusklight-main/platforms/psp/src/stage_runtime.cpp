#include "dusk/psp/stage_runtime.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace dusk::psp::stage {

bool safe_room_id(const RoomId& id) {
    if (id.room > 63 || id.layer < -1 || id.layer > 14 ||
        id.stage[0] == '\0' || id.stage[8] != '\0') {
        return false;
    }
    bool terminated = false;
    for (std::uint32_t index = 0; index < 8; ++index) {
        const char value = id.stage[index];
        if (value == '\0') {
            terminated = true;
            continue;
        }
        if (terminated ||
            !((value >= 'A' && value <= 'Z') ||
              (value >= '0' && value <= '9') ||
              value == '_')) {
            return false;
        }
    }
    return true;
}

bool derive_room_path(
    const RoomId& id,
    const char* filename,
    char* output,
    std::uint32_t capacity) {
    if (!safe_room_id(id) || filename == nullptr ||
        output == nullptr || capacity == 0 ||
        std::strchr(filename, '/') != nullptr ||
        std::strstr(filename, "..") != nullptr) {
        return false;
    }
    const int count = std::snprintf(
        output, capacity, "data/stages/%s/R%02u/%s",
        id.stage, id.room, filename);
    return count > 0 && static_cast<std::uint32_t>(count) < capacity;
}

bool RoomResourceManager::load_room(
    const RoomId& wanted, const RoomPackages& packages) {
    if (loaded || !safe_room_id(wanted)) {
        return false;
    }
    RoomPackages checked = {};
    if (room::validate_dprm(
            packages.model.bytes, packages.model.size,
            &checked.model) != room::PackageError::Ok ||
        room::validate_room_dptx(
            packages.textures.bytes, packages.textures.size,
            &checked.textures) != room::PackageError::Ok ||
        room::validate_dpcl(
            packages.collision.bytes, packages.collision.size,
            &checked.collision) != room::PackageError::Ok ||
        room::validate_dpsc(
            packages.scene.bytes, packages.scene.size,
            &checked.scene) != room::PackageError::Ok) {
        return false;
    }
    const char* source_stage =
        reinterpret_cast<const char*>(checked.scene.bytes + 176);
    if (std::strncmp(source_stage, wanted.stage, 8) != 0 ||
        room::read_u32(checked.scene.bytes + 20) != wanted.room) {
        return false;
    }
    current = checked;
    id = wanted;
    std::memset(&room_switches, 0, sizeof(room_switches));
    loaded = true;
    ++load_calls;
    return true;
}

bool RoomResourceManager::validate_room() const {
    return loaded &&
           current.model.bytes != nullptr &&
           current.textures.bytes != nullptr &&
           current.collision.bytes != nullptr &&
           current.scene.bytes != nullptr;
}

bool RoomResourceManager::activate_room() {
    if (!validate_room() || active) {
        return false;
    }
    ++generation;
    ++activate_calls;
    active = true;
    return true;
}

void RoomResourceManager::deactivate_room() {
    if (active) {
        active = false;
        ++deactivate_calls;
    }
}

void RoomResourceManager::unload_room() {
    if (!loaded) {
        return;
    }
    deactivate_room();
    current = {};
    loaded = false;
    ++unload_calls;
}

bool RoomResourceManager::handle_valid(const RoomHandle& handle) const {
    return active && handle.pointer != nullptr &&
           handle.generation == generation;
}

RoomHandle RoomResourceManager::collision_handle() const {
    return {generation, active ? current.collision.bytes : nullptr};
}

bool RoomTransitionController::request(
    const RoomResourceManager& resources,
    std::uint16_t exit_index,
    const float player_position[3]) {
    if (state != State::Playing || !resources.active ||
        player_position == nullptr ||
        room::read_u16(resources.current.scene.bytes + 4) < 3) {
        ++rejected_requests;
        return false;
    }
    const std::uint32_t count =
        room::read_u32(resources.current.scene.bytes + 212);
    room::SceneExitV3 selected = {};
    bool found = false;
    for (std::uint32_t index = 0; index < count; ++index) {
        if (room::read_dpsc_exit_v3(
                resources.current.scene, index,
                &selected) == room::PackageError::Ok &&
            selected.source_exit_index == exit_index) {
            found = true;
            break;
        }
    }
    if (!found || selected.trigger_index == 0xffff ||
        room::read_dpsc_trigger_v3(
            resources.current.scene, selected.trigger_index,
            &trigger) != room::PackageError::Ok ||
        trigger.exit_index != exit_index) {
        ++rejected_requests;
        return false;
    }
    const float dx = player_position[0] - trigger.position[0];
    const float dy = player_position[1] - trigger.position[1];
    const float dz = player_position[2] - trigger.position[2];
    const float yaw =
        static_cast<float>(trigger.rotation[1]) *
        (6.28318530717958647692f / 65536.0f);
    const float local_x = std::cos(yaw) * dx - std::sin(yaw) * dz;
    const float local_z = std::sin(yaw) * dx + std::cos(yaw) * dz;
    if (std::fabs(local_x) > trigger.dimensions[0] ||
        dy < 0.0f || dy > trigger.dimensions[1] ||
        std::fabs(local_z) > trigger.dimensions[2]) {
        ++rejected_requests;
        return false;
    }
    destination = selected;
    state = State::TransitionRequested;
    return true;
}

void RoomTransitionController::begin_fade_out() {
    if (state == State::TransitionRequested) {
        state = State::FadeOut;
        fade_update = 0;
    }
}

bool RoomTransitionController::update_fade() {
    if (state != State::FadeOut && state != State::FadeIn) {
        return false;
    }
    ++fade_update;
    return fade_update >= 20;
}

void RoomTransitionController::begin_loading() {
    state = State::LoadingDestination;
    fade_update = 0;
}

void RoomTransitionController::begin_fade_in() {
    state = State::FadeIn;
    fade_update = 0;
}

void RoomTransitionController::complete() {
    state = State::Playing;
    ++transition_count;
}

void RoomTransitionController::fail() {
    state = State::Error;
    ++failures;
}

bool RoomTransitionController::transition_active() const {
    return state != State::Playing && state != State::Paused &&
           state != State::Boot && state != State::Error &&
           state != State::Exiting;
}

bool PspStageRuntime::boot(
    const RoomId& id, const RoomPackages& packages) {
    transition.state = State::LoadingInitialRoom;
    if (!resources.load_room(id, packages) ||
        !resources.activate_room()) {
        transition.fail();
        return false;
    }
    transition.state = State::Playing;
    return true;
}

bool PspStageRuntime::request_transition(
    std::uint16_t exit_index, const float player_position[3]) {
    if (paused) {
        ++transition.rejected_requests;
        return false;
    }
    return transition.request(resources, exit_index, player_position);
}

void PspStageRuntime::set_paused(bool value) {
    if (transition.transition_active()) {
        return;
    }
    paused = value;
    transition.state = value ? State::Paused : State::Playing;
}

bool PspStageRuntime::consistent() const {
    return stale_actor_handles == 0 &&
           stale_texture_references == 0 &&
           transition.failures == 0 &&
           (!resources.active || resources.loaded);
}

}  // namespace dusk::psp::stage
