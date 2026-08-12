#ifndef DUSK_PSP_ORIGINAL_SCENE_EXIT_BRIDGE_HPP
#define DUSK_PSP_ORIGINAL_SCENE_EXIT_BRIDGE_HPP

#include "dusk/psp/process_runtime.hpp"
#include "dusk/psp/room_package.hpp"

#include <cstdint>

namespace dusk::psp::compat {

using TransitionRequest = bool (*)(
    void* user, std::uint8_t exit_index,
    std::uint8_t path_id, bool jump,
    const float player_position[3]);
using SwitchQuery = bool (*)(
    void* user, std::int8_t room, std::uint8_t number);
using SwitchWrite = void (*)(
    void* user, std::int8_t room, std::uint8_t number);
using EventBitQuery = bool (*)(void* user, std::uint16_t flag);
using TreasureQuery = bool (*)(void* user, std::uint8_t number);

struct SceneExitFacade {
    void* user;
    TransitionRequest request_transition;
    SwitchQuery is_switch;
    SwitchWrite on_switch;
    SwitchWrite off_switch;
    EventBitQuery is_event_bit;
    TreasureQuery is_treasure_open;
};

struct SceneExitSnapshot {
    std::uint32_t parameters;
    float position[3];
    std::int16_t rotation[3];
    float dimensions[3];
    std::int8_t room;
};

void bind_scene_exit_facade(const SceneExitFacade& facade);
void unbind_scene_exit_facade();
void set_scene_exit_player_position(const float position[3]);
void set_scene_exit_paused(bool paused);

bool register_original_scene_exit_profile(
    process::PspProcessManager* manager);
bool create_original_scene_exit(
    process::PspProcessManager* manager,
    const room::PackageView& scene,
    std::int8_t room,
    process::ProcessHandle* handle);
bool inspect_original_scene_exit(
    const process::PspProcessManager& manager,
    process::ProcessHandle handle,
    SceneExitSnapshot* snapshot);

std::uint32_t original_transition_request_calls();
std::uint32_t specialized_psp_trigger_logic_calls();
bool original_profile_valid();

}  // namespace dusk::psp::compat

#endif
