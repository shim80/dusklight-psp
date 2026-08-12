#ifndef DUSK_PSP_ENVIRONMENT_RUNTIME_HPP
#define DUSK_PSP_ENVIRONMENT_RUNTIME_HPP

#include "dusk/psp/room_package.hpp"

#include <cstdint>

namespace dusk::psp::environment {

enum class Error : std::uint32_t {
    Ok,
    InvalidPackage,
    RoomMismatch,
    StageMismatch,
    Generation,
    NonFinite,
};

struct PspMaterialEnvironmentState {
    std::uint32_t ambient;
    std::uint32_t diffuse;
    float key_light_direction[3];
    std::uint32_t key_light_color;
    float local_light_position[3];
    std::uint32_t local_light_color;
    float local_light_power;
    bool local_light_enabled;
    bool fog_enabled;
    float fog_near;
    float fog_far;
    std::uint32_t fog_color;
    std::uint32_t clear_color;
    float shadow_factor;
    float shadow_direction[3];
    std::uint32_t emissive;
    std::uint8_t alpha;
};

struct Metrics {
    std::uint32_t records_loaded;
    std::uint32_t transitions_started;
    std::uint32_t transitions_completed;
    std::uint32_t stale_generations_rejected;
    std::uint32_t non_finite_values;
    std::uint32_t allocations_during_playing;
    std::uint32_t local_lights_used;
    bool source_derived;
    bool fog_enabled;
};

struct PspEnvironmentRuntime {
    room::EnvironmentRecordV4 active;
    room::EnvironmentRecordV4 target;
    PspMaterialEnvironmentState material;
    Metrics metrics;
    std::uint32_t generation;
    float transition;
    float transition_duration;
    bool loaded;
    bool active_valid;
    bool transitioning;

    void initialize();
    Error load(
        const room::PackageView& scene,
        std::uint32_t expected_stage_hash,
        std::uint32_t expected_room,
        std::uint32_t room_generation);
    Error activate(std::uint32_t room_generation);
    Error update(float delta_seconds, std::uint32_t room_generation);
    void unload(std::uint32_t room_generation);
    bool consistent(std::uint32_t room_generation) const;
};

}  // namespace dusk::psp::environment

#endif
