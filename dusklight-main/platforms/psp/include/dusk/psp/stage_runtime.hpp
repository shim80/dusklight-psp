#ifndef DUSK_PSP_STAGE_RUNTIME_HPP
#define DUSK_PSP_STAGE_RUNTIME_HPP

#include "dusk/psp/room_package.hpp"

#include <cstdint>

namespace dusk::psp::stage {

enum class State : std::uint8_t {
    Boot,
    LoadingInitialRoom,
    Playing,
    TransitionRequested,
    FadeOut,
    UnloadingRoom,
    LoadingDestination,
    SpawningPlayer,
    CreatingActors,
    FadeIn,
    Paused,
    Error,
    Exiting,
};

struct RoomId {
    char stage[9];
    std::uint8_t room;
    std::int8_t layer;
};

struct RoomPackages {
    room::PackageView model;
    room::PackageView textures;
    room::PackageView collision;
    room::PackageView scene;
};

struct PersistentDemoState {
    std::uint8_t hearts;
    std::uint8_t maximum_hearts;
    std::uint32_t rupees;
    std::uint32_t global_updates;
    bool debug_visible;
};

struct RoomSwitchBank {
    std::uint32_t bits[8];
};

struct StageSwitchBank {
    std::uint32_t bits[8];
};

struct RoomHandle {
    std::uint32_t generation;
    const void* pointer;
};

class RoomResourceManager {
public:
    bool load_room(const RoomId& id, const RoomPackages& packages);
    bool validate_room() const;
    bool activate_room();
    void deactivate_room();
    void unload_room();
    bool handle_valid(const RoomHandle& handle) const;
    RoomHandle collision_handle() const;

    RoomPackages current = {};
    RoomId id = {};
    RoomSwitchBank room_switches = {};
    std::uint32_t generation = 0;
    std::uint32_t load_calls = 0;
    std::uint32_t unload_calls = 0;
    std::uint32_t activate_calls = 0;
    std::uint32_t deactivate_calls = 0;
    std::uint32_t stale_handles = 0;
    bool loaded = false;
    bool active = false;
};

class RoomTransitionController {
public:
    bool request(
        const RoomResourceManager& resources,
        std::uint16_t exit_index,
        const float player_position[3]);
    void begin_fade_out();
    bool update_fade();
    void begin_loading();
    void begin_fade_in();
    void complete();
    void fail();
    bool transition_active() const;

    State state = State::Boot;
    room::SceneExitV3 destination = {};
    room::SceneTriggerV3 trigger = {};
    std::uint32_t fade_update = 0;
    std::uint32_t transition_count = 0;
    std::uint32_t rejected_requests = 0;
    std::uint32_t failures = 0;
};

class PspStageRuntime {
public:
    bool boot(const RoomId& id, const RoomPackages& packages);
    bool request_transition(
        std::uint16_t exit_index, const float player_position[3]);
    void set_paused(bool paused);
    bool consistent() const;

    RoomResourceManager resources = {};
    RoomTransitionController transition = {};
    PersistentDemoState player = {3, 3, 0, 0, false};
    StageSwitchBank stage_switches = {};
    std::uint32_t actor_create_calls = 0;
    std::uint32_t actor_destroy_calls = 0;
    std::uint32_t stale_actor_handles = 0;
    std::uint32_t stale_texture_references = 0;
    bool paused = false;
};

bool safe_room_id(const RoomId& id);
bool derive_room_path(
    const RoomId& id,
    const char* filename,
    char* output,
    std::uint32_t capacity);

}  // namespace dusk::psp::stage

#endif
